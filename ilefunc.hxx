/*
 * Copyright (c) 2025 Seiden Group
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

extern "C" {
	#include <as400_protos.h>
	#include <unistd.h>
}

#include <array>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <type_traits>
#if !defined(__cpp_lib_logical_traits)
#include <experimental/type_traits> // just assume for GCC 6, no polyfill
#endif

namespace pase_cpp {

#if defined(__cpp_lib_logical_traits)
template<typename... Args>using Disjunction = std::disjunction<Args...>;
#else
template<typename... Args>using Disjunction = std::experimental::disjunction<Args...>;
#endif

/*
 * Type-safe compile-time wrapper for IBM i ILE service programs. ILE passes
 * arguments by value; the PASE interface requires them to be passed through
 * memory. PASE pointers are automatically converted into ILEpointer structs
 * with correct alignment.
 *
 * Thanks to Saagar Jha for help with the building the arglist.
 */

/*
 * ParameterPackContains is used for asserting ILEFunction's arglist won't
 * contain void. (std::disjunction isn't C++14, but GCC 6, the lowest version
 * that we support, does have it, albeit under experimental.)
 */
template<class What, class... Args>
struct ParameterPackContains : Disjunction<std::is_same<What, Args>...> {};

/**
 * Templated utility functions for type-specific handling in ILE FFI.
 */
template<typename T>
class ILEArgument {
public:
	ILEArgument() = delete;

	static constexpr size_t align() {
		return alignof(T);
	}

	static constexpr arg_type_t type() {
		return sizeof(T);
	}

	static constexpr result_type_t result_type() {
		return sizeof(T);
	}

	static T base_return(ILEarglist_base*);

	// This is an overload on ILEArgument instead of in ILEArglist to
	// work around an issue with GCC pre-10 confusing T and T*.
	// Do not provide for void, which is for returns only;
	// this method will error on void. ILEFunction will block void.
	template <typename TInner = T, typename = typename std::enable_if_t<false == std::is_void<TInner>::value>>
	static inline void write(char *dst, TInner src) {
		// XXX: Should this be with tags?
                memcpy(dst, &src, sizeof(src));
        }
};

/*
 * Handles returning fundamental types from an ILEarglist's result union.
 * For aggregates and pointers, returning values is more complicated and
 * involves setting the aggregate pointer.
 *
 * The proper fix for this would be changing it to use a { TReturn ret;
 * return ret; } format (with necessary void specialization), but perfect
 * NRVO isn't just for performance, it's to avoid destroying the tags on
 * copying. Because this is tricky to guarantee, even for the simplest case
 * (even the simplest impl will have different addresses in/out of a func),
 * we just support the pointer-to case for fundamental types.
 *
 * For now, this is defined on ILEArgument rather than ILEArglist due to
 * limitations in partial template specialization.
 */
#define DefineBaseReturnSubst(T, accessor) \
	template<> T ILEArgument<T>::base_return(ILEarglist_base *base){return base->result. accessor;}
template<> void ILEArgument<void>::base_return(ILEarglist_base*){return;}
DefineBaseReturnSubst(int8_t, s_int8.r_int8);
DefineBaseReturnSubst(uint8_t, s_uint8.r_uint8);
DefineBaseReturnSubst(int16_t, s_int16.r_int16);
DefineBaseReturnSubst(uint16_t, s_uint16.r_uint16);
DefineBaseReturnSubst(int32_t, s_int32.r_int32);
DefineBaseReturnSubst(uint32_t, s_uint32.r_uint32);
DefineBaseReturnSubst(int64_t, r_int64);
DefineBaseReturnSubst(uint64_t, r_uint64);
DefineBaseReturnSubst(float, r_float64);
DefineBaseReturnSubst(double, r_float64);
#undef DefineBaseReturnSubst

#define DefineResultType(T, ret) \
	template<>constexpr result_type_t ILEArgument<T>::result_type(){return ret;}
DefineResultType(void, RESULT_VOID);
DefineResultType(int8_t, RESULT_INT8);
DefineResultType(uint8_t, RESULT_UINT8);
DefineResultType(int16_t, RESULT_INT16);
DefineResultType(uint16_t, RESULT_UINT16);
DefineResultType(int32_t, RESULT_INT32);
DefineResultType(uint32_t, RESULT_UINT32);
DefineResultType(int64_t, RESULT_INT64);
DefineResultType(uint64_t, RESULT_UINT64);
DefineResultType(float, RESULT_FLOAT64);
DefineResultType(double, RESULT_FLOAT64);
#undef DefineResultType

#define DefineArgumentType(T, ret) \
	template<>constexpr arg_type_t ILEArgument<T>::type(){return ret;}
DefineArgumentType(int8_t, ARG_INT8);
DefineArgumentType(uint8_t, ARG_UINT8);
DefineArgumentType(int16_t, ARG_INT16);
DefineArgumentType(uint16_t, ARG_UINT16);
DefineArgumentType(int32_t, ARG_INT32);
DefineArgumentType(uint32_t, ARG_UINT32);
DefineArgumentType(int64_t, ARG_INT64);
DefineArgumentType(uint64_t, ARG_UINT64);
DefineArgumentType(float, ARG_FLOAT32);
DefineArgumentType(double, ARG_FLOAT64);
// XXX: Teraspace, space, open pointers
#undef DefineArgumentType

template<typename T>
class ILEArgument<T*> {
public:
	static constexpr size_t align() {
		// Use sizeof in case we're using older IBM i headers
		// without the correct alignment attributes for GCC.
		return sizeof(ILEpointer);
	}

	static constexpr arg_type_t type() {
		return ARG_MEMPTR;
	}

	static inline void write(char *dst, T *src) {
		// The high word should be unset as it's initialized in ctor
                ILEpointer *p = (ILEpointer*)dst;
                p->s.addr = (address64_t)src;
        }
};

/**
 * Builds an argument list compatible with _ILECALL from a parameter pack.
 */
template <typename... TArgs>
class ILEArglist {
	using Sizes = std::array<std::size_t, sizeof...(TArgs)>;
public:
	ILEArglist(TArgs... args) {
		this->base = {};
		this->arguments = {};
		write<0>(args...);
	}

	static constexpr auto offsets() {
		auto sizes = Sizes{ILEArgument<TArgs>::align()...};
		auto offsets = Sizes();
		// const& is required for [] and back to be constexpr in C++14
		const Sizes &sizes_c = sizes;
		const Sizes &offsets_c = offsets;
		// Like an exclusive scan, but align to the preferred alignment of the type
		for (size_t i = 0; i < sizes.size(); i++) {
			if (i == 0) {
				continue;
			}
			const size_t base = (sizes_c[i - 1] + offsets_c[i - 1]);
			const size_t aligned = (base + sizes_c[i] - 1) & -(sizes_c[i]);
			const_cast<typename Sizes::reference>(offsets_c[i]) = aligned;
		}
		return offsets;
	}

	static constexpr auto size() {
		if (sizeof...(TArgs) == 0) {
			return (size_t)0;
		}
		auto sizes = Sizes{ILEArgument<TArgs>::align()...};
		auto offsets = ILEArglist::offsets();
		const Sizes &sizes_c = sizes;
		const Sizes &offsets_c = offsets;
		return sizes_c.back() + offsets_c.back();
	}

	__attribute__((aligned(16))) ILEarglist_base base;
	__attribute__((aligned(16))) std::array<char, size()> arguments;
private:
	template <size_t index>
	void write() {
	}

	template <size_t index, typename T, typename... Ts>
	void write(T argument, Ts... rest) {
		ILEArgument<T>::write(this->arguments.data() + offsets()[index], argument);
		write<index + 1>(rest...);
	}
};

template<typename TReturn, typename... TArgs>
class ILEFunction {
	static_assert(ParameterPackContains<void, TArgs...>::value == false, "Argument list must not contain void (use no args instead)");
	static_assert(sizeof...(TArgs) <= 400, "_ILECALL maximum arguments reached");

	using ActivationMark = unsigned long long;
public:
	ILEFunction(const char *path, const char *symbol, int flags = 0) {
		this->my_pid = -1;
		this->activation_mark = -1;
		this->procedure = {};
		// Invalid flags will result in... ILECALL_INVALID_FLAGS
		this->flags = flags & (ILECALL_NOINTERRUPT | ILECALL_EXCP_NOSIGNAL);
		this->path = path;
		this->symbol = symbol;
		this->signature = {
			ILEArgument<TArgs>::type()..., ARG_END
		};
	}

	void init() {
		// Forking will destroy the activation mark
		pid_t current_pid = getpid();
		if (this->my_pid == current_pid) {
			return;
		}
		this->activation_mark = _ILELOADX(this->path.c_str(), ILELOAD_LIBOBJ);
		if (this->activation_mark == (ActivationMark)-1) {
			throw std::invalid_argument("invalid service program");
		}
		if (_ILESYMX(&this->procedure, this->activation_mark, this->symbol.c_str()) != ILESYM_PROCEDURE) {
			throw std::invalid_argument("invalid symbol");
		}
		this->my_pid = current_pid;
	}

	/**
	 * Call the ILE function.
	 *
	 * This overload is used for fundamental (void/arith) types.
	 */
	template <typename TReturnInner = TReturn, typename = typename std::enable_if_t<(ILEArgument<TReturnInner>::result_type() <= 0)>>
	TReturnInner operator()(TArgs... args) {
		// can just be ILEArglist(args...) in C++17
		auto arguments = ILEArglist<TArgs...>(args...);
		this->call(&arguments.base);
		// XXX: Tagged pointers
		return ILEArgument<TReturnInner>::base_return(&arguments.base);
	}

	/**
	 * Call the ILE function.
	 *
	 * This overload is used for structures, due to limitations of NRVO.
	 */
	template <typename TReturnInner = TReturn, typename = typename std::enable_if_t<(ILEArgument<TReturnInner>::result_type() > 0)>>
	void operator()(TReturnInner *ret, TArgs... args) {
		auto arguments = ILEArglist<TArgs...>(args...);
		arguments.base.result.r_aggregate.s.addr = (address64_t)ret;
		this->call(&arguments.base);
	}

private:
	void call(ILEarglist_base *base) {
		// Lazy initialization (and reinit), throws
		this->init();
		int rc = _ILECALLX(&this->procedure, base, this->signature.data(), ILEArgument<TReturn>::result_type(), this->flags);
		// 0 is OK, -1 w/ errno 3474 is an MI exception, positive is _ILECALL error
		// These shouldn't happen with our wrapper around it.
		if (rc == ILECALL_INVALID_ARG) {
			throw std::invalid_argument("invalid signature");
		} else if (rc == ILECALL_INVALID_RESULT) {
			throw std::invalid_argument("invalid result");
		} else if (rc == ILECALL_INVALID_FLAGS) {
			throw std::invalid_argument("invalid flags");
		}
	}

	ActivationMark activation_mark;
	pid_t my_pid;
	int flags;
	ILEpointer procedure __attribute__ ((aligned (16)));
	std::string path, symbol;
	std::array<arg_type_t, sizeof...(TArgs) + 1> signature;
};

}

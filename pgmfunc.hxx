// vim: expandtab:ts=2:sw=2
/*
 * Copyright (c) 2025 Seiden Group
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

extern "C" {
#include <as400_protos.h>
}

#include <stdexcept>
#include <type_traits>
#if !defined(__cpp_lib_logical_traits)
#include <experimental/type_traits> // just assume for GCC 6, no polyfill
#endif

namespace pase_cpp {

#if defined(__cpp_lib_logical_traits)
template <typename... Args> using Conjunction = std::conjunction<Args...>;
#else
template <typename... Args>
using Conjunction = std::experimental::conjunction<Args...>;
#endif

/*
 * Type-safe compile-time wrapper for IBM i *PGM objects. The OPM calling
 * convention is pass-by-reference; this automatically passes value types
 * passed to the function into pointers to the stack.
 */

/*
 * Because _PGMCALL's ASCII mode takes only strings, and can't write back
 * to them, we'll want to check to make sure you're passing constant
 * strings to it. Maybe check for char* too?
 */
template <class What, class... Args>
struct ParameterPackAllAre : Conjunction<std::is_same<What, Args>...> {};

// If passed by value: get the reference on the stack (and de-constify)
template <typename T> constexpr void *ParameterArrayMember(T &obj) {
  return static_cast<void *>(&obj);
}
template <typename T> constexpr void *ParameterArrayMember(const T &obj) {
  return ParameterArrayMember(const_cast<T &>(obj));
}
// If it's already a pointer, just pass it through
template <typename T> constexpr void *ParameterArrayMember(T *obj) {
  return static_cast<void *>(obj);
}
template <typename T> constexpr void *ParameterArrayMember(const T *obj) {
  return ParameterArrayMember(const_cast<T *>(obj));
}

template <typename... TArgs> class PGMFunction {
public:
  PGMFunction(const char *library, const char *object, int flags = 0) {
    static_assert(sizeof...(TArgs) <= 16383,
                  "_PGMCALL maximum arguments reached");

    this->flags = process_flags(flags);
    // Unlike ILE handles, these survive forks
    if (_RSLOBJ2(&this->pgm, RSLOBJ_TS_PGM, object, library)) {
      throw std::invalid_argument("invalid program");
    }
  }
  int operator()(TArgs... args) {
    void *pgm_argv[] = {ParameterArrayMember(args)..., NULL};
    return _PGMCALL(&this->pgm, pgm_argv, this->flags);
  }

private:
  constexpr int process_flags(int flags) {
    if (sizeof...(TArgs) > PGMCALL_MAXARGS) {
      flags |= PGMCALL_NOMAXARGS;
    }
    // Can't be a static assert here, flags is non-constexpr
    if (ParameterPackAllAre<const char *, TArgs...>::value == false &&
        (flags & PGMCALL_ASCII_STRINGS)) {
      throw std::invalid_argument(
          "all values must be const char* for ASCII strings flag");
    }
    // XXX: PGMCALL_DIRECT_ARGS requires different argv type
    return flags &
           (PGMCALL_DROP_ADOPT | PGMCALL_NOINTERRUPT | PGMCALL_NOMAXARGS |
            PGMCALL_ASCII_STRINGS | PGMCALL_EXCP_NOSIGNAL);
  }

  ILEpointer pgm __attribute__((aligned(16)));
  int flags;
};

} // namespace pase_cpp

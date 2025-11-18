# PASE C++ wrapper

This is a collection of header-only libraries to make wrapping native IBM i
libraries from PASE safer and easier, with no boilerplate. It includes:

* `ebcdic.hxx`: Utilities for dealing with EBCDIC string/char constants.
* `ilefunc.hxx`: Wraps ILE functions from service programs.
* `pgmfunc.hxx`: Wraps OPM programs.

Currently, C++14 w/ GCC 6 is targetted.

## Examples

Some example programs are provided for how to use the wrappers and to test
their functionality.

* `examples/convpath`: Calls the `Qp0lCvtPathToQSYSObjName` ILE function to
  convert IFS `/QSYS.LIB` paths to traditional notation.
* `examples/sysval`: Calls the `QWCRSVAL` program to get system values.

Some more trivial examples to test functionality are also in the examples
folders.

* `examples/float`: Calls the ILE `pow` function and compares it to PASE `pow`.
* `examples/getpid`: Calls the ILE `getpid` function and compares it to PASE `getpid`.
* `examples/struct`: Tests aggregate returns. More complicated, requires manual intervention (for now) to build a ILE C service program.

`make examples` will build these in `examples/`.

## Usage

All of these are in the namespace `pase_cpp`. You can use `using namespace`
to import everything (useful for the `_e` literal), or qualify the full name.

### `ebcdic.hxx`

A user-defined literal for EBCDIC characters is defined; this is useful for
zoned numerics used for booleans:

```cpp
char yes = '1'_e; // 0xF1, EBCDIC '1'
```

A wrapper for EBCDIC fixed-length strings is also defined.

```cpp

EF<8> QSYS0100("QSYS0100");

const char *ebcdic = QSYS0100.value; // 0xD8, 0xE2, 0xE8, 0xE2, 0xF0, 0xF1, 0xF0, 0xF0, 0x00
```

Note that the interface for this is a little awkward due to limitations in
C++14; this will likely be revised when we no longer need C++14 compatibility.

### `ilefunc.hxx`

An ILE function is defined by its return type and optionally any arguments.

```cpp
// int getpid(void); in ILE
auto ile_getpid = ILEFunction<int>("QSYS/QP0WSRV1", "getpid");
auto ilepid = ile_getpid();
fprintf(stderr, "Return value: '%d'\n", ilepid);

// int mult(int, int); in ILE
auto mult = ILEFunction<int, int, int>("CALVIN/TESTILE", "mult");
auto ret = mult(10, 11);
fprintf(stderr, "Return value: '%d'\n", ret); // 110

// void increment(char*, int); in ILE
auto increment = ILEFunction<void, char*, int>("CALVIN/TESTILE", "increment");
char str[] = "foobar";
increment(str, 3);
fprintf(stderr, "Return value: '%s'\n", str); // gppbar
```

PASE pointer arguments are automatically lifted to ILEpointer for you.

Aggregate/pointer returns are more complicated. Because structures contain
tags, we want to avoid copying, not just for performance, but tag integrity.
Because C++ named value return optimizations aren't perfect, for now, this
class only supports returning structures with an additional prepended pointer
of where the return structure should be written to. Note that pointers are
equivalent to the `PASE ILEpointer`, so those in your structure (and return
type) should be that structure and not a C pointer type, unless you change
the ILE C options for ABI to use i.e. 64-bit teraspace pointers instead.

(In the future, having the aggregate case work with normal return values may
be supported, at least with non-pointers or some kind of guaranteed NRVO with
a fixed address.)

```cpp
// Ensure this structure is compatible with your structure in ILE C;
// pointers will be ILEpointer (unless you change the ILE C ABI)
struct example { /* ... */ };

auto f = ILEFunction<struct example, int>("CALVIN/TESTILE", "func");
struct example example1;
// Not example1 = f(42);
f(&example1, 42);
```

Currently, teraspace, space, and open pointers arguments aren't supported yet.

### `pgmfunc.hxx`

An OPM program passes by reference; value types for input will be automatically
be turned into pointers.

```cpp
PGMFunction<char*, int, const char*, const char*, void*> QWCRTVTZ("QSYS", "QWCRTVTZ");

char buffer[8192];
// Not depicted: converting these into EBCDIC
const char *format_name = "RTMZ0100";
const char *timezone_name = "QN0500EST3";
// Not depicted: Qus_EC_t from qusec.h
Qus_EC_t error = {};
error.Bytes_Provided = sizeof(error);

QWCRTVTZ(buffer, sizeof(buffer), format_name, timezone_name, &error);
```


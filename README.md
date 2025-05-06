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

`make examples` will build these in `examples/`.

## Usage

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

PASE pointers are automatically lifted to ILEpointer for you.

Currently, teraspace, space, and open pointers, nor aggregate returns aren't
yet supported.

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


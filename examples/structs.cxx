// vim: expandtab:ts=2:sw=2
/*
 * Copyright (c) 2025 Seiden Group
 *
 * SPDX-License-Identifier: ISC
 */
 
#include <cstdio>

extern "C" {
#include </QOpenSys/usr/include/iconv.h>
}

#include "ilefunc.hxx"

using namespace pase_cpp;

struct example {
  long long a;
  long long b;
  long long c;
  long long d;
  long long e;
};

/*
 * ILE C function:
 *   struct example func(int i)
 *   {
 *           struct example x;
 *           x.a = i + 0;
 *           x.b = i + 1;
 *           x.c = i + 2;
 *           x.d = i + 3;
 *           x.e = i + 4;
 *           return x;
 *   }
 * build with:
 *   crtcmod localetype(*localeutf) srcstmf('testile.c') module(calvin/testile)
 *   crtsrvpgm srvpgm(calvin/testile) module(calvin/testile) export(*all)
 */

int main(void) {
  auto f = pase_cpp::ILEFunction<struct example, int>("CALVIN/TESTILE", "func");
  struct example example1;
  f(&example1, 42); // not example1 = f(42);
  printf("struct 1: %lld %lld %lld %lld %lld\n", example1.a, example1.b,
         example1.c, example1.d, example1.e);
  return 0;
}

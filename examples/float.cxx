// vim: expandtab:ts=2:sw=2
/*
 * Copyright (c) 2025 Seiden Group
 *
 * SPDX-License-Identifier: ISC
 */
 
#include <cmath>
#include <cstdio>

extern "C" {
#include </QOpenSys/usr/include/iconv.h>
}

#include "ilefunc.hxx"

using namespace pase_cpp;

int main(void) {
  auto ile_pow =
      pase_cpp::ILEFunction<double, double, double>("QSYS/QC2UTIL1", "pow");
  double ile_two_to_eight = ile_pow(2.0, 8.0);
  double two_to_eight = pow(2.0, 8.0);
  printf("ILE %f AIX %f\n", ile_two_to_eight, two_to_eight);
  return 0;
}

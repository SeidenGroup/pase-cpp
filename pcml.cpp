// vim: expandtab:ts=2:sw=2
/*
 * Copyright (c) 2026 Seiden Group
 *
 * SPDX-License-Identifier: ISC
 */
 
#include <cstdio>
#include <string>

extern "C" {
#include <fcntl.h>
#include </QOpenSys/usr/include/iconv.h>
}

#include "ebcdic.hxx"
#include "ilefunc.hxx"
#include "pgmfunc.hxx"

using namespace pase_cpp;

/* Structures for Qp0lCvtPathToQSYSObjName */
typedef struct Qus_EC {
  int Bytes_Provided;
  int Bytes_Available;
  char Exception_Id[7];
  char Reserved;
} Qus_EC_t;

typedef struct __attribute__((packed)) Qlg_Path_Name {
  int CCSID;
  char Country_ID[2];
  char Language_ID[3];
  char Reserved[3];
  unsigned int Path_Type;
  int Path_Length;
  char Path_Name_Delimiter[2];
  char Reserved2[10];
} Qlg_Path_Name_T;

typedef struct QSYS0100 {
  int bytes_returned;
  int bytes_available;
  int ccsid_out;
  char lib_name[28];
  char lib_type[20];
  char obj_name[28];
  char obj_type[20];
  char mbr_name[28];
  char mbr_type[20];
  char asp_name[28];
} QSYS0100;

constexpr auto QSYS0100_name = "QSYS0100"_e;

static auto Qp0lCvtPathToQSYSObjName =
    ILEFunction<void, Qlg_Path_Name_T *, QSYS0100 *, const char *, unsigned int,
                unsigned int, Qus_EC_t *>("QSYS/QP0LLIB2",
                                          "Qp0lCvtPathToQSYSObjName");

/* Structures for QBNRPII */
typedef struct __attribute__((packed)) Qbn_Interface_Entry
{
  int  Offset_Next_Entry;                /* Offset from start of receiver */
  char Module_Name[10];
  char Module_Library[10];
  int  Interface_Info_CCSID;
  int  Interface_Info_Type;
  int  Offset_Interface_Info;            /* Offset from start of receiver */
  int  Interface_Info_Length_Ret;        /* Bytes returned                */
  int  Interface_Info_Length_Avail;      /* Bytes available to be returned*/
  /*char Reserved1[];              */    /* Varying length                */
  /*char Qbn_Interface_Info[];     */    /* Interface_Info_Length         */
  /*char Reserved2[];              */    /* Varying length                */
} Qbn_Interface_Entry_t;

typedef struct __attribute__((packed)) Qbn_PGII0100
{
  int  Bytes_Returned;
  int  Bytes_Available;
  char Obj_Name[10];
  char Obj_Lib_Name[10];
  char Obj_Type[10];
  char Reserved3[2];
  int  Offset_First_Entry;               /* Offset from start of receiver */
  int  Number_Entries;
  /*char Reserved4[];             */     /* Varying length                */
  /*Qbn_Interface_Entry_t List[]; */     /* Repeated for each entry       */
} Qbn_PGII0100_t;

constexpr auto RPII0100_name = "RPII0100"_e;
constexpr EbcdicFixedString<20> ALLBNDMOD("*ALLBNDMOD");

static auto QBNRPII =
    PGMFunction<char *, int, const char *, const char *, const char *, const char *, Qus_EC_t *>("QSYS", "QBNRPII");

/* Program */

iconv_t to_37, from_37;

void get_pcml(const char *path)
{
  Qus_EC_t err = {};
  err.Bytes_Provided = sizeof(err);

  struct {
    Qlg_Path_Name_T qlg;
    char path[1024];
  } input_qlg = {};

  char *in = (char *)path, *out = input_qlg.path;
  size_t inleft = strlen(path), outleft = 1024;
  iconv(to_37, &in, &inleft, &out, &outleft);

  input_qlg.qlg.CCSID = 37;
  input_qlg.qlg.Path_Length = strlen(input_qlg.path);
  input_qlg.qlg.Path_Name_Delimiter[0] = '/'_e;

  QSYS0100 qsys = {};
  qsys.bytes_available = sizeof(qsys);

  Qp0lCvtPathToQSYSObjName(&input_qlg.qlg, &qsys, QSYS0100_name, sizeof(qsys),
                           37, &err);
  if (err.Exception_Id[0] != '\0') {
    char exception_id[8];
    in = (char *)err.Exception_Id;
    inleft = 7;
    out = exception_id;
    outleft = 8;
    iconv(from_37, &in, &inleft, &out, &outleft);
    exception_id[7] = '\0';
    fprintf(stderr, "Failed to convert %s: %s\n", path, exception_id);
    return;
  }

  /****/

  char libobj[20];
  memcpy(libobj, qsys.obj_name, 10);
  memcpy(libobj + 10, qsys.lib_name, 10);
  // Ensure filename is space and not null padded
  for (int i = 0; i < 20; i++) {
    if (libobj[i] == '\0') {
      libobj[i] = ' '_e;
    }
  }
  // Same for the type...
  char type[10];
  memcpy(type, qsys.obj_type, 10);
  for (int i = 0; i < 10; i++) {
    if (type[i] == '\0') {
      type[i] = ' '_e;
    }
  }

  char buf[10000];
  Qbn_PGII0100_t *ptr = (Qbn_PGII0100_t*)buf;
  ptr->Bytes_Available = sizeof(buf);
  err = {};
  err.Bytes_Provided = sizeof(err);
  QBNRPII(buf, sizeof(buf), RPII0100_name, libobj, type, ALLBNDMOD, &err);
  if (err.Exception_Id[0] != '\0') {
    char exception_id[8];
    in = (char *)err.Exception_Id;
    inleft = 7;
    out = exception_id;
    outleft = 8;
    iconv(from_37, &in, &inleft, &out, &outleft);
    exception_id[7] = '\0';
    fprintf(stderr, "Failed to get program info: %s\n", exception_id);
    return;
  }

  fprintf(stderr, "%s: %d bytes returned, %d entries\n", path, ptr->Bytes_Returned, ptr->Number_Entries);
}

int main(int argc, char **argv)
{
  to_37 = iconv_open(ccsidtocs(37), ccsidtocs(Qp2paseCCSID()));
  from_37 = iconv_open(ccsidtocs(Qp2paseCCSID()), ccsidtocs(37));

  for (int i = 1; i < argc; i++) {
    get_pcml(argv[i]);
  }

  return 0;
}

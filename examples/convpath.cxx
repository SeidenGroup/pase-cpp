#include <cstdio>
#include <cstring>

extern "C" {
	#include </QOpenSys/usr/include/iconv.h>
	#include <limits.h>
}

#include "ebcdic.hxx"
#include "ilefunc.hxx"

typedef struct Qus_EC {
	int  Bytes_Provided;
	int  Bytes_Available;
	char Exception_Id[7];
	char Reserved;
} Qus_EC_t;

typedef struct __attribute__((packed)) Qlg_Path_Name {
	int  CCSID;
	char Country_ID[2];
	char Language_ID[3];
	char Reserved[3];
	unsigned int Path_Type;
	int  Path_Length;
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

EF<8> QSYS0100_name("QSYS0100");

static auto Qp0lCvtPathToQSYSObjName = ILEFunction<void, Qlg_Path_Name_T*, QSYS0100*, const char*, unsigned int, unsigned int, Qus_EC_t*>("QSYS/QP0LLIB2", "Qp0lCvtPathToQSYSObjName");

iconv_t to_37, from_37;

static void to_ascii(char *ebcdic, size_t ebcdic_size, char *ascii, size_t ascii_out)
{
	const char *ascii_begin = ascii;
	iconv(from_37, &ebcdic, &ebcdic_size, &ascii, &ascii_out);
	// Trim spaces
	while (ascii >= ascii_begin && *(ascii - 1) == ' ') {
		*ascii-- = '\0';
	}
}

static void print_qsys(const char *path)
{
	Qus_EC_t err = {};
	err.Bytes_Provided = sizeof(err);
	
	struct {
		Qlg_Path_Name_T qlg;
		char path[1024];
	} input_qlg = {};

	char *in = (char*)path, *out = input_qlg.path;
	size_t inleft = strlen(path), outleft = 1024;
	iconv(to_37, &in, &inleft, &out, &outleft);

	input_qlg.qlg.CCSID = 37;
	input_qlg.qlg.Path_Length = strlen(input_qlg.path);
	input_qlg.qlg.Path_Name_Delimiter[0] = '/'_e;
	
	QSYS0100 qsys = {};
	qsys.bytes_available = sizeof(qsys);

	Qp0lCvtPathToQSYSObjName(&input_qlg.qlg, &qsys, QSYS0100_name.value, sizeof(qsys), 37, &err);
	if (err.Exception_Id[0] != '\0') {
		char exception_id[8];
		in = (char*)err.Exception_Id;
		inleft = 7;
		out = exception_id;
		outleft = 8;
		iconv(from_37, &in, &inleft, &out, &outleft);
		fprintf(stderr, "Failed to convert %s: %s\n", path, exception_id);
		return;
	}

	// Handle worst case UTF-8 conversion
	char lib_name[60], obj_name[60], mbr_name[60];
	to_ascii(qsys.lib_name, 10, lib_name, 60);
	to_ascii(qsys.obj_name, 10, obj_name, 60);
	to_ascii(qsys.mbr_name, 10, mbr_name, 60);
	if (strlen(mbr_name) > 0) {
		printf("%s: %s/%s(%s)\n", path, lib_name, obj_name, mbr_name);
	} else {
		printf("%s: %s/%s\n", path, lib_name, obj_name);
	}
}

int main(int argc, char **argv)
{
	to_37 = iconv_open(ccsidtocs(37), ccsidtocs(Qp2paseCCSID()));
	from_37 = iconv_open(ccsidtocs(Qp2paseCCSID()), ccsidtocs(37));

	for (int i = 1; i < argc; i++) {
		print_qsys(argv[i]);
	}

	return 0;
}

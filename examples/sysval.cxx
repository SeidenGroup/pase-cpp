#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
	#include </QOpenSys/usr/include/iconv.h>
}

#include "ebcdic.hxx"
#include "pgmfunc.hxx"

using namespace pase_cpp;

typedef struct Qus_EC {
	int  Bytes_Provided;
	int  Bytes_Available;
	char Exception_Id[7];
	char Reserved;
} Qus_EC_t;

typedef struct Qwc_Rsval_Sys_Value_Table {
	char System_Value[10];
	char Type_Data;
	char Information_Status;
	int  Length_Data;
	char Data[];	    /* Varying length		   */
} Qwc_Rsval_Sys_Value_Table_t;

typedef struct Qwc_Rsval_Data_Rtnd {
	int  Number_Sys_Vals_Rtnd;
	int  Offset_Sys_Val_Table[];/* Varying length	      */
     /*Qwc_Rsval_Sys_Value_Table_t System_Values[];*//* Varying length */
} Qwc_Rsval_Data_Rtnd_t;

static auto QWCRSVAL = PGMFunction<void*, int, int, char*, Qus_EC_t*>("QSYS", "QWCRSVAL");

static iconv_t to_37, from_37;

static void to_ascii(char *ebcdic, size_t ebcdic_size, char *ascii, size_t ascii_out)
{
	iconv(from_37, &ebcdic, &ebcdic_size, &ascii, &ascii_out);
}

static void print_sysval(Qwc_Rsval_Sys_Value_Table *sysval)
{
	char name[60] = {};
	to_ascii(sysval->System_Value, 10, name, 60);

	if (sysval->Information_Status == 'L'_e) {
		fprintf(stderr, "%s: locked\n", name);
		return;
	}

	if (sysval->Type_Data != 'C'_e) {
		fprintf(stderr, "%s: not a string\n", name);
		return;
	}

	auto out = std::string(sysval->Length_Data * 6, '\0');
	to_ascii(sysval->Data, sysval->Length_Data, (char*)out.data(), out.capacity());
	printf("%s: %s\n", name, out.c_str());
}

int main(int argc, char **argv)
{
	to_37 = iconv_open(ccsidtocs(37), ccsidtocs(Qp2paseCCSID()));
	from_37 = iconv_open(ccsidtocs(Qp2paseCCSID()), ccsidtocs(37));

	if (argc < 2) {
		return 1;
	}

	int sysval_count = argc - 1;
	std::vector<char[10]> sysvals(sysval_count);
	for (int i = 1; i < argc; i++) {
		char *in = argv[i];
		char *out = sysvals[i - 1];
		size_t inleft = strlen(in), outleft = 10;
		iconv(to_37, &in, &inleft, &out, &outleft);
		// Pad with spaces...
		while (outleft > 0) {
			outleft--;
			*out++ = ' '_e;
		}
	}

	Qus_EC_t err = {};
	err.Bytes_Provided = sizeof(err);
	
	// Awkward because of variable length structs
	char buffer[16 * 1024] = {};
	//fwrite(sysvals.data(), 10, sysval_count, stdout);
	QWCRSVAL(buffer, sizeof(buffer), sysval_count, (char*)sysvals.data(), &err);
	if (err.Exception_Id[0] != '\0') {
		char exception_id[8];
		char *in = (char*)err.Exception_Id, *out = exception_id;
		size_t inleft = 7, outleft = 8;
		iconv(from_37, &in, &inleft, &out, &outleft);
		fprintf(stderr, "Failed to get system values: %s\n", exception_id);
		return 1;
	}

	Qwc_Rsval_Data_Rtnd_t *returned = (Qwc_Rsval_Data_Rtnd_t*)buffer;
	for (int i = 0; i < returned->Number_Sys_Vals_Rtnd; i++) {
		int offset = returned->Offset_Sys_Val_Table[i];
		print_sysval((Qwc_Rsval_Sys_Value_Table_t*)(buffer + offset));
	}

	return 0;
}

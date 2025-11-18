#include <cstdio>

extern "C" {
	#include </QOpenSys/usr/include/iconv.h>
	#include <unistd.h>
}

#include "ilefunc.hxx"

using namespace pase_cpp;

int main(int argc, char **argv)
{
	auto ile_getpid = pase_cpp::ILEFunction<int>("QSYS/QP0WSRV1", "getpid");
	pid_t ile_pid = ile_getpid();
	pid_t pid = getpid();
	printf("ILE %d AIX %d\n", ile_pid, pid);
	return 0;
}

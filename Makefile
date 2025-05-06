CXX := $(shell if type g++-10 > /dev/null 2> /dev/null; then echo g++-10; else echo g++; fi)
LD := $(CXX)
CXXFLAGS := -I$(PWD) -std=c++14 -Wall -Wextra -O2
LDFLAGS :=

.PHONY: all clean examples

all: examples

examples: examples/convpath examples/sysval

examples/sysval: examples/sysval.o
	$(LD) $(LDFLAGS) -o $@ $^ /QOpenSys/usr/lib/libiconv.a

examples/convpath: examples/convpath.o
	$(LD) $(LDFLAGS) -o $@ $^ /QOpenSys/usr/lib/libiconv.a

clean:
	rm -f *.o examples/*.o examples/convpath examples/sysval

%.o: %.cxx
	$(CXX) $(CXXFLAGS)  -c -o $@ $^

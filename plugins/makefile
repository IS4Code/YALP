GPP = g++ -m32 -std=c++11 -Ilib -Isrc -fno-stack-protector -nodefaultlibs
GCC = gcc -m32 -Ilib -Isrc -fno-stack-protector -nodefaultlibs
LINK = $(GCC)
PP_OUTFILE = "./YALP.so"

COMPILE_FLAGS = -c -O3 -fPIC -w -DLINUX -pthread -fno-operator-names

YALP = -D YALP $(COMPILE_FLAGS)

all: YALP

clean:
	-rm -f *~ *.o *.so
  
static: GPP = g++ -m32 -std=c++11 -Ilib -Isrc -fno-stack-protector -static-libgcc -static-libstdc++
static: GCC = gcc -m32 -Ilib -Isrc -fno-stack-protector -static-libgcc -static-libstdc++
static: LINK = $(GPP)
static: all

YALP: clean
	$(GPP) $(YALP) ./lib/sdk/*.cpp
	$(GCC) $(YALP) ./lib/subhook/*.c
	$(GCC) $(YALP) ./lib/lua/*.c
	$(GPP) $(YALP) ./src/*.cpp
	$(LINK) -fshort-wchar -pthread -shared -o $(PP_OUTFILE) *.o

GPP = g++ -D _GLIBCXX_USE_CXX11_ABI=0 -m32 -std=c++11 -Ilib -Isrc -fno-stack-protector
GCC = gcc -D _GLIBCXX_USE_CXX11_ABI=0 -m32 -Ilib -Isrc -fno-stack-protector
LINK = $(GPP) -lstdc++
PP_OUTFILE = "./YALP.so"

COMPILE_FLAGS = -c -O3 -fPIC -w -DLINUX -pthread -fno-operator-names

YALP = -D YALP $(COMPILE_FLAGS)

all: YALP

clean:
	-rm -f *~ *.o *.so
  
static: GPP = g++ -D _GLIBCXX_USE_CXX11_ABI=0 -m32 -std=c++11 -Ilib -Isrc -fno-stack-protector -static-libgcc -static-libstdc++
static: GCC = gcc -D _GLIBCXX_USE_CXX11_ABI=0 -m32 -Ilib -Isrc -fno-stack-protector -static-libgcc -static-libstdc++
static: LINK = $(GPP)
static: all

YALP: clean
	$(GPP) $(YALP) ./lib/sdk/*.cpp
	$(GCC) $(YALP) ./lib/subhook/*.c
	$(GPP) $(YALP) ./lib/lua/*.cpp
	$(GPP) $(YALP) ./src/amx/*.cpp
	$(GPP) $(YALP) ./src/lua/interop/*.cpp
	$(GPP) $(YALP) ./src/lua/*.cpp
	$(GPP) $(YALP) ./src/*.cpp
	$(LINK) -fshort-wchar -pthread -shared -o $(PP_OUTFILE) *.o

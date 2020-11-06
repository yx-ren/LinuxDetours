UNAME := $(shell uname)
UNAME_M := $(shell uname -m)

CXX = g++
CXX_FLAGS = -Wall -Wno-null-conversion -std=c++11 -g -O0 -fno-omit-frame-pointer -fPIC -shared
CXX_EXE_FLAGS = -Wall -std=c++11 -g -O0 -Wno-null-conversion -fno-omit-frame-pointer
CXX_DEFINES = -lpthread -ldl -lglog 

SOURCES = barrier.cpp detours.cpp disasm.cpp plthook_elf.cpp
#PREPROCESS = $(CXX) -E detours.cpp -o detours.E

HOOK_SOURCE_FILES = hook.c statistics.cpp DetourDetector.cpp

TRAMPOLINE_ARM = trampoline_arm.cpp
TRAMPOLINE_X86 = trampoline_x86.cpp
TRAMPOLINE_MIPS = trampoline_mips.cpp

LIB_NAME = libdetours

.PHONY: linux_arm linux_arm64 linux_x64 macos_x64 linux_mips64

all:
ifeq ($(UNAME),Darwin)
	$(MAKE) macos_x64
else
    ifeq ($(UNAME_M),x86_64)
		$(MAKE) linux_x64
    else ifeq ($(UNAME_M),mips64)
		$(MAKE) linux_mips64
    else ifneq (,$(findstring arm,$(UNAME_M)))
		$(MAKE) linux_arm
    else
		$(MAKE) linux_arm64
    endif
endif

target_app = "linux_detours.test"
linux_x64:
	$(CXX) $(CXX_FLAGS) $(CXX_DEFINES) $(SOURCES) $(TRAMPOLINE_X86) -D_AMD64_ -D_UNIX64 -o $(LIB_NAME)64.so	
	$(CXX) $(CXX_EXE_FLAGS) main.cpp $(CXX_DEFINES) -L ./ -L/usr/lib/x86_64-linux-gnu -ldetours64 \
		-Wl,-rpath=./ \
		-o $(target_app)
	$(CXX) -E detours.cpp -D_AMD64_ -D_UNIX64 -o detours.E
	$(CXX) -c $(CXX_FLAGS) $(TRAMPOLINE_X86) -D_AMD64_ -D_UNIX64 -o $(TRAMPOLINE_X86).o
	$(CXX) $(CXX_FLAGS) $(CXX_DEFINES) $(HOOK_SOURCE_FILES) -D_AMD64_ -D_UNIX64 -o hook.so \
		-L ./ -L/usr/lib/x86_64-linux-gnu -ldetours64 \
		-Wl,-rpath=/usr/local/lib/ \
		-Wl,-rpath=./
linux_arm:
	$(CXX) $(CXX_FLAGS) $(CXX_DEFINES) $(SOURCES) $(TRAMPOLINE_ARM) -mcpu=cortex-a5 -D_ARM_ -D_ARM32_ -o $(LIB_NAME)32.so
linux_arm64:
	$(CXX) $(CXX_FLAGS) $(CXX_DEFINES) $(SOURCES) $(TRAMPOLINE_ARM) -D_ARM64_ -D_UNIX64 -o $(LIB_NAME)64.so
linux_mips64:
	$(CXX) $(CXX_FLAGS) $(CXX_DEFINES) $(SOURCES) $(TRAMPOLINE_MIPS) -D_MIPS64_ -D_UNIX64 -o $(LIB_NAME)64.so
	$(CXX) $(CXX_EXE_FLAGS) main.cpp $(CXX_DEFINES)  -L ./ -L/usr/lib/x86_64-linux-gnu -ldetours64 -Wl,-rpath=./ -Wl,-rpath=/usr/local/lib/ \
		-o $(target_app)
	$(CXX) -E detours.cpp -D_MIPS64_ -D_UNIX64 -o detours.E
	$(CXX) -c $(CXX_FLAGS) $(TRAMPOLINE_MIPS) -D_MIPS64_ -D_UNIX64 -o $(TRAMPOLINE_MIPS).o
	$(CXX) $(CXX_FLAGS) $(CXX_DEFINES) $(HOOK_SOURCE_FILES) -D_MIPS64_ -D_UNIX64 -o hook.so \
		-L ./ -L/usr/lib/x86_64-linux-gnu -ldetours64 \
		-Wl,-rpath=/usr/local/lib/ \
		-Wl,-rpath=./
macos_x64:
	$(CXX) $(CXX_FLAGS) $(CXX_DEFINES) $(SOURCES) $(TRAMPOLINE_X86) -D_AMD64_ -D_UNIX64 -o $(LIB_NAME)64.dylib

clean:
	rm -f $(target_app)
ifeq ($(UNAME),Darwin)
	rm -f $(LIB_NAME)64.dylib
else
    ifneq (,$(findstring arm,$(UNAME_M)))
			rm -f $(LIB_NAME)32.so			
    else 
			rm -f $(LIB_NAME)64.so
    endif	
endif


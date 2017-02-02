OS := $(shell uname -s)

# Variables with default values
CXX?=g++
EXEC?=MonaTiny

# Variables extendable
CFLAGS+=-D_GLIBCXX_USE_C99 -std=c++11 -Wall -Wno-reorder -Wno-unknown-pragmas
override INCLUDES+=-I../MonaBase/include/ -I../MonaCore/include/ -I./include -I../
LIBDIRS+=-L../MonaBase/lib/ -L../MonaCore/lib/
LDFLAGS+="-Wl,-rpath,../MonaBase/lib/,-rpath,../MonaCore/lib/,-rpath,/usr/local/lib/"
LIBS+=-pthread -lMonaBase -lMonaCore -lcrypto -lssl
ifneq ($(OS),FreeBSD)
	LIBS+= -ldl
endif
ifeq ($(OS),Darwin)
	LBITS := $(shell getconf LONG_BIT)
	ifeq ($(LBITS),64)
	   # just require for OSX 64 buts
		LIBS +=  -pagezero_size 10000 -image_base 100000000
	endif
endif

# Variables fixed
SOURCES = $(wildcard $(SRCDIR)sources/*.cpp)
OBJECT = $(SOURCES:sources/%.cpp=tmp/Release/%.o)
OBJECTD = $(SOURCES:sources/%.cpp=tmp/Debug/%.o)

# pre-build => versionning
$(shell if [ -d "../.git/hooks" ] && [ ! -h "../.git/hooks/pre-commit" ]; then ln -s "../git.hooks.pre-commit" "../.git/hooks/pre-commit"; fi;)

# This line is used to ignore possibly existing folders Release/Debug
.PHONY: release debug

release:
	mkdir -p tmp/Release/
	@$(MAKE) -k $(OBJECT)
	@echo creating executable $(EXEC)
	@$(CXX) $(CFLAGS) -O2 $(LDFLAGS) $(LIBDIRS) -o $(EXEC) $(OBJECT) $(LIBS)

debug:	
	mkdir -p tmp/Debug/
	@$(MAKE) -k $(OBJECTD)
	@echo creating debug executable $(EXEC)
	@$(CXX) -g -D_DEBUG $(CFLAGS) -Og $(LDFLAGS) $(LIBDIRS) -o $(EXEC) $(OBJECTD) $(LIBS)

$(OBJECT): tmp/Release/%.o: sources/%.cpp
	@echo compiling $(@:tmp/Release/%.o=sources/%.cpp)
	@$(CXX) $(CFLAGS) $(INCLUDES) -c -o $(@) $(@:tmp/Release/%.o=sources/%.cpp)

$(OBJECTD): tmp/Debug/%.o: sources/%.cpp
	@echo compiling $(@:tmp/Debug/%.o=sources/%.cpp)
	@$(CXX) -g -D_DEBUG $(CFLAGS) $(INCLUDES) -c -o $(@) $(@:tmp/Debug/%.o=sources/%.cpp)

clean:
	@echo cleaning project $(EXEC)
	@rm -f $(OBJECT) $(EXEC)
	@rm -f $(OBJECTD) $(EXEC)

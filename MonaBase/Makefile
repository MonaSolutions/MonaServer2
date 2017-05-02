OS := $(shell uname -s)

# Variables with default values
CXX?=g++

# Variables extendable
CFLAGS+=-D_GLIBCXX_USE_C99 -std=c++11 -Wall -Wno-reorder -Wno-terminate -Wunknown-pragmas -Wno-multichar -D_FILE_OFFSET_BITS=64
override INCLUDES+=-I./include/
LIBS+=-lcrypto -lssl

# Variables fixed
ifeq ($(OS),Darwin)
	LIB=lib/libMonaBase.dylib
	SHARED=-dynamiclib -install_name ./../MonaBase/$(LIB)
else
	LIB=lib/libMonaBase.so
	AR=lib/libMonaBase.ar
	SHARED=-shared
endif
SOURCES = $(filter-out sources/Win%.cpp, $(wildcard $(SRCDIR)sources/*.cpp))
OBJECT = $(SOURCES:sources/%.cpp=tmp/release/%.o)
OBJECTD = $(SOURCES:sources/%.cpp=tmp/debug/%.o)

release:
	mkdir -p tmp/release
	mkdir -p lib
	@$(MAKE) -k $(OBJECT)
	@echo creating dynamic lib $(LIB)
	@$(CXX) $(CFLAGS) -O2 -fPIC $(SHARED) -o $(LIB) $(OBJECT) $(LIBS)
	@echo creating static lib $(AR)
	@ar rcs $(AR) $(OBJECT)

debug:
	mkdir -p tmp/debug
	mkdir -p lib
	@$(MAKE) -k $(OBJECTD)
	@echo creating dynamic debug lib $(LIB)
	@$(CXX) -g -D_DEBUG $(CFLAGS) -Og -fPIC $(SHARED) -o $(LIB) $(OBJECTD) $(LIBS)
	@echo creating static lib $(AR)
	@ar rcs $(AR) $(OBJECTD)

$(OBJECT): tmp/release/%.o: sources/%.cpp
	@echo compiling $(@:tmp/release/%.o=sources/%.cpp)
	@$(CXX) $(CFLAGS) -fpic $(INCLUDES) -c -o $(@) $(@:tmp/release/%.o=sources/%.cpp)

$(OBJECTD): tmp/debug/%.o: sources/%.cpp
	@echo compiling $(@:tmp/debug/%.o=sources/%.cpp)
	@$(CXX) -g -D_DEBUG $(CFLAGS) -fpic $(INCLUDES) -c -o $(@) $(@:tmp/debug/%.o=sources/%.cpp)

clean:
	@echo cleaning project MonaBase
	@rm -f $(OBJECT) $(LIB) $(AR)
	@rm -f $(OBJECTD) $(LIB) $(AR)

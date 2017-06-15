# Constants
OS = $(shell uname -s)
ifeq ($(shell printf '\1' | od -dAn | xargs),1)
	BIG_ENDIAN = 0
else
	BIG_ENDIAN = 1
endif

# Variables with default values
CXX?=g++

# Variables extendable
CFLAGS+=-D_GLIBCXX_USE_C99 -std=c++11 -Wall -Wno-reorder -Wno-terminate -Wunknown-pragmas -Wno-unknown-warning-option -Wno-overloaded-virtual -Wno-potentially-evaluated-expression -D__BIG_ENDIAN__=$(BIG_ENDIAN) -D_FILE_OFFSET_BITS=64
override INCLUDES+=-I./../MonaBase/include/ -I./include/
LIBDIRS+=-L./../MonaBase/lib/
LIBS+=-lMonaBase -lcrypto -lssl

# Variables fixed
ifeq ($(OS),Darwin)
    LIB=lib/libMonaCore.dylib
	SHARED=-dynamiclib -install_name ./../MonaCore/$(LIB)
else
	LIB=lib/libMonaCore.so
	SHARED=-shared
endif


SOURCES = $(wildcard sources/*.cpp sources/*/*.cpp)
OBJECT = $(addprefix tmp/release/,$(notdir $(SOURCES:%.cpp=%.o)))
OBJECTD = $(addprefix tmp/debug/,$(notdir $(SOURCES:%.cpp=%.o)))

release:
	mkdir -p tmp/release
	mkdir -p lib
	@$(MAKE) -k $(OBJECT)
	@echo creating dynamic lib $(LIB)
	@$(CXX) $(CFLAGS) -O2 $(LIBDIRS) -fPIC $(SHARED) -o $(LIB) $(OBJECT) $(LIBS)

debug:
	mkdir -p tmp/debug
	mkdir -p lib
	@$(MAKE) -k $(OBJECTD)
	@echo creating dynamic debug lib $(LIB)
	@$(CXX) -g -D_DEBUG $(CFLAGS) -Og $(LIBDIRS) -fPIC $(SHARED) -o $(LIB) $(OBJECTD) $(LIBS)

$(OBJECT):
	@echo compiling $(filter %/$(subst .o,.cpp,$(notdir $(@))),$(SOURCES))
	@$(CXX) $(CFLAGS) -fpic $(INCLUDES) -c -o $(@) $(filter %/$(subst .o,.cpp,$(notdir $(@))),$(SOURCES))

$(OBJECTD):
	@echo compiling $(filter %/$(subst .o,.cpp,$(notdir $(@))),$(SOURCES))
	@$(CXX) -g -D_DEBUG $(CFLAGS) -fpic $(INCLUDES) -c -o $(@) $(filter %/$(subst .o,.cpp,$(notdir $(@))),$(SOURCES))

clean:
	@echo cleaning project MonaCore
	@rm -f $(OBJECT) $(LIB)
	@rm -f $(OBJECTD) $(LIB)

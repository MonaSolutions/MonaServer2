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
override CFLAGS+=-D_GLIBCXX_USE_C99 -std=c++14 -D__BIG_ENDIAN__=$(BIG_ENDIAN) -D_FILE_OFFSET_BITS=64 -Wall -Wno-reorder -Wno-terminate -Wunknown-pragmas -Wno-unknown-warning-option -Wno-overloaded-virtual -Wno-potentially-evaluated-expression -Wno-switch-bool -Wno-exceptions
override INCLUDES+=-I./../MonaBase/include/ -I./include/ -I/usr/local/opt/openssl/include/
override LIBDIRS+=-L./../MonaBase/lib/
override LIBS+=-lMonaBase -lcrypto -lssl
ifneq ("$(wildcard /usr/local/opt/openssl/lib/)","")
	override LIBDIRS+=-L/usr/local/opt/openssl/lib/
endif
ifdef ENABLE_SRT
	override CFLAGS += -DENABLE_SRT
endif

# Variables fixed
ifeq ($(OS),Darwin)
	LIB=lib/libMonaCore.dylib
	SHARED=-dynamiclib -install_name @executable_path/../MonaCore/$(LIB)
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
	@$(CXX) $(CFLAGS) -O3 $(LIBDIRS) -fPIC $(SHARED) -o $(LIB) $(OBJECT) $(LIBS)

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

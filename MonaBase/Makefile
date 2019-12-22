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
override CFLAGS+=-D_GLIBCXX_USE_C99 -std=c++14 -D__BIG_ENDIAN__=$(BIG_ENDIAN) -D_FILE_OFFSET_BITS=64 -Wall -Wno-reorder -Wno-terminate -Wunknown-pragmas -Wno-unknown-warning-option -Wno-exceptions 
override INCLUDES+=-I./include/ -I/usr/local/opt/openssl/include/
override LIBS+=-lcrypto -lssl
ifneq ("$(wildcard /usr/local/opt/openssl/lib/)","")
	override LIBDIRS+=-L/usr/local/opt/openssl/lib/
endif
ifdef ENABLE_SRT
	override CFLAGS += -DENABLE_SRT
	override LIBS += -lsrt
endif

# Variables fixed
ifeq ($(OS),Darwin)
	LIB=lib/libMonaBase.dylib
	SHARED=-dynamiclib -install_name ./../MonaBase/$(LIB)
else
	LIB=lib/libMonaBase.so
	AR=lib/libMonaBase.a
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
	@$(CXX) $(CFLAGS) -O3 $(LIBDIRS) -fPIC $(SHARED) -o $(LIB) $(OBJECT) $(LIBS)
	@if [ "$(OS)" != "Darwin" ]; then\
		echo "creating static lib $(AR)";\
		ar rcs $(AR) $(OBJECT);\
	fi

debug:
	mkdir -p tmp/debug
	mkdir -p lib
	@$(MAKE) -k $(OBJECTD)
	@echo creating dynamic debug lib $(LIB)
	@$(CXX) -g -D_DEBUG $(CFLAGS) -Og $(LIBDIRS) -fPIC $(SHARED) -o $(LIB) $(OBJECTD) $(LIBS)
	@if [ "$(OS)" != "Darwin" ]; then\
		echo "creating static lib $(AR)";\
		ar rcs $(AR) $(OBJECTD);\
	fi
   
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

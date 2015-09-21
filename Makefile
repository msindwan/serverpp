# SPP Makefile
# Author : Mayank Sindwani
# Date : 2015-09-19

CXX       = g++
CXXFLAGS  = -static-libgcc -static-libstdc++ -g -std=c++11 -I include/ -I ext/jconf/include -I $(OPENSSL_DIR)/include

IO_OBJECTS = $(patsubst %.cpp, %.o, $(wildcard src/io/*.cpp))

DEPENDS  = -Llib -lsppio -Llib -ljconf
LIB_DIR  = lib
BIN_DIR  = bin
EXEC     = serverpp
IO_LIB   = libsppio.a

ifeq ($(OS),Windows_NT)
	SVC_OBJECTS = $(patsubst %.cpp, %.o, $(wildcard src/service/win/*.cpp))
	DEPENDS    += -lws2_32 -lWtsapi32 $(OPENSSL_DIR)/lib/MinGW/libeay32.a $(OPENSSL_DIR)/lib/MinGW/ssleay32.a
	CXXFLAGS   += -DSPP_WINDOWS
else
	SVC_OBJECTS = $(patsubst %.cpp, %.o, $(wildcard src/service/linux/*.cpp))
	CXXFLAGS   += -DSPP_LINUX
endif

target: io
target: $(SVC_OBJECTS)
	#
	# Build JConf
	#
	@make -C ext/jconf/
	@cp ext/jconf/lib/libjconf.a lib
	#
	# Build Serverpp.srv
	#
	@mkdir -p $(BIN_DIR)
	$(CXX) -static-libgcc -static-libstdc++ -o $(BIN_DIR)/$(EXEC) $(SVC_OBJECTS) $(DEPENDS)

io: $(IO_OBJECTS)
	#
	# Build Serverpp.io
	#
	@mkdir -p $(LIB_DIR)
	ar rcs $(LIB_DIR)/$(IO_LIB) $(IO_OBJECTS)

clean:
	rm -rf $(IO_OBJECTS) $(SVC_OBJECTS) $(BIN_DIR) $(LIB_DIR)
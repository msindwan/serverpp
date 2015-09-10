# SPP Makefile
# Author : Mayank Sindwani
# Date : 2015-07-18

CC       = g++
CPPFLAGS = -I include/ -I ext/jconf/include

IO_OBJECTS  = src/io/log.o src/io/socket.o src/io/http.o src/io/util.o

DEPENDS  = -Llib -lsppio -Llib -ljconf
LIB_DIR  = lib
BIN_DIR  = bin
EXEC     = serverpp
LIB      = libsppio.a

WIN_SRV_OBJECTS = $(patsubst %.cpp, %.o, $(wildcard src/service/win32/*.cpp))
WIN_DEPENDS = $(DEPENDS) -lws2_32

target: io
target: $(WIN_SRV_OBJECTS)
	#
	# Build JConf
	#
	@make -C ext/jconf/
	@cp ext/jconf/lib/libjconf.a lib
	#
	# Build Serverpp.srv
	#
	@mkdir -p $(BIN_DIR)
	$(CC) -static-libgcc -static-libstdc++ -o $(BIN_DIR)/$(EXEC) $(WIN_SRV_OBJECTS) $(WIN_DEPENDS)

io: $(IO_OBJECTS)
	#
	# Build Serverpp.io
	#
	@mkdir -p $(LIB_DIR)
	$(AR) rcs $(LIB_DIR)/$(LIB) $(IO_OBJECTS)

clean:
	rm -rf $(IO_OBJECTS) $(WIN_SRV_OBJECTS) $(SRV_OBJECTS) $(BIN_DIR)
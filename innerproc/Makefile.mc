all: innerproc.mc upgrade.mc start_innerproc.mc
.PHONY: clean
TARGET = innerproc.mc
HEADERS = cJSON.h dictionary.h sqlite3.h jsonrpc-c.h innerproc.h rc4.h ycapic.h ev.h ifaddrs.h
OBJECTS = innerproc.o cJSON.o jsonrpc-c.o rc4.o ifaddrs.o request.o app_client.o

ACC= ${CC}

ALDFLAGS = ${LDFLAGS}
#LIBS= -lev -lm -lsqlite3 -ldl -liniparser -lycapic
LIBS= -lev -lm -lsqlite3 -ldl -liniparser -lyajl_s
#DEBUG= -g
CFLAGS= -Wall -fPIE -pie 
DEFINE= -DSQL -DANDROID -DMAICHONG -DNURSE -D_APP_WATCHDOG -DALL_IN_ONE
LINK= -L . -L/usr/local/lib
INCLUDE= -I . -I/usr/local/include

%.o: %.c $(HEADERS)
	$(ACC) $(DEFINE) $(INCLUDE) $(ALDFLAGS) -c $< -o $@ 

$(TARGET): $(OBJECTS) 
	$(ACC) $(DEBUG) $(DEFINE) $(OBJECTS) iniparser.a libsqlite3.a -o $@ $(LINK) $(INCLUDE) $(LIBS) $(CFLAGS) $(ALDFLAGS)
	arm-linux-androideabi-strip $(TARGET)
	
upgrade.mc: upgrade.o md5sum.o
	$(ACC) $(DEFINE) -o upgrade.mc upgrade.o md5sum.o $(LINK) $(INCLUDE) $(CFLAGS) $(ALDFLAGS)
	arm-linux-androideabi-strip upgrade.mc

start_innerproc.mc: start_innerproc.o cJSON.o jsonrpc-c.o rc4.o
	$(ACC) $(DEFINE) -o start_innerproc.mc start_innerproc.o cJSON.o jsonrpc-c.o rc4.o $(LINK) $(INCLUDE) $(LIBS) $(CFLAGS) $(ALDFLAGS)
	arm-linux-androideabi-strip start_innerproc.mc

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)
	rm -f upgrade.mc start_innerproc.mc
	rm -f *.o

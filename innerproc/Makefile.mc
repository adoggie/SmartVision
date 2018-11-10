TARGET = innerproc.mc
HEADERS = cJSON.h dictionary.h sqlite3.h jsonrpc-c.h innerproc.h rc4.h ycapic.h ev.h ifaddrs.h
#OBJECTS = innerproc.o cJSON.o jsonrpc-c.o rc4.o ifaddrs.o sqlite3.o #sqlitedbproc.o
OBJECTS = innerproc.o cJSON.o jsonrpc-c.o rc4.o  sqlite3.o #sqlitedbproc.o

ACC= ${CC}
ACC=$(CCPREFIX)gcc
ALDFLAGS = ${LDFLAGS}
#LIBS= -lev -lm -lsqlite3 -ldl -liniparser -lycapic
LIBS= -lev -lm  -ldl -liniparser -lpthread -lc 

#DEBUG= -g
#CFLAGS= -Wall -fPIE -pie 
CFLAGS= -Wall -fPIC
DEFINE= -DSQL -DANDROID -DMAICHONG -DNURSE
LINK= -L .
INCLUDE= -I .

%.o: %.c $(HEADERS)
	$(ACC) $(DEFINE) $(INCLUDE) $(ALDFLAGS) -c $< -o $@ 

$(TARGET): $(OBJECTS) 
	$(ACC) $(DEBUG) $(DEFINE) $(OBJECTS)   -o $@ $(LINK) $(INCLUDE) $(LIBS) $(CFLAGS) $(ALDFLAGS)

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)

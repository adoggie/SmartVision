TARGET = outerproc
HEADERS = cJSON.h dictionary.h elevator_control.h iniparser.h jsonrpc-c.h outerproc.h rc4.h
OBJECTS = outerproc.o cJSON.o jsonrpc-c.o rc4.o

CC= ${CCPREFIX}gcc

#LIBS = -L./ -lev -lm -lpthread 
#LIB_PATH=/home/smart/arm-linux-gcc/arm-linux-gnueabihf/lib
LIBS =   -L. -lm -lpthread  -lev -liniparser -L/home/smart/arm-linux-gcc/arm-linux-gnueabihf/lib
DEBUG = -g
CFLAGS = -Wall
DEFINE = -DBEIJING

%.o: %.c $(HEADERS)
	$(CC) $(DEBUG) $(DEFINE) $(LIBS) -c $< -o $@

$(TARGET): $(OBJECTS) 
	$(CC) $(DEBUG) $(DEFINE) $(OBJECTS)  -o $@ $(LIBS) $(CFLAGS) 
	#$(CC) $(DEBUG) $(DEFINE) $(OBJECTS) libiniparser.a -o $@ $(LIBS) $(CFLAGS)

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)


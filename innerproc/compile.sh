$CC -g ifaddrs.c innerproc.c cJSON.c jsonrpc-c.c rc4.c libsqlite3.a iniparser.a -o innerproc  -DSQL -DANDROID -fPIE -pie -lev -lm -liniparser -lycapic -L . -I . -lsqlite3 $LDFLAGS

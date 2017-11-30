CC=gcc

CFLAGS=$(shell pkg-config --cflags gconf-2.0)
LIBS=$(shell pkg-config --libs gconf-2.0)

#CFLAGS+= -g -Wall -I.
CFLAGS+= -O6 -I. -Wall -U_FORTIFY_SOURCE

all: miniobd2d obdecode

miniobd2d: miniobd2d.c
	gcc -o $@ $^ $(LIBS) $(CFLAGS)

clean:
	rm -f miniobd2d obdecode

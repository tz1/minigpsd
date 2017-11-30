CC=gcc

CFLAGS=$(shell pkg-config --cflags gconf-2.0)
LIBS=$(shell pkg-config --libs gconf-2.0)

#CFLAGS+= -g -Wall -I.
CFLAGS+= -O6 -I. -Wall -U_FORTIFY_SOURCE

all: miniv1d

miniv1d: miniv1d.c
	gcc -o $@ $^ $(LIBS) $(CFLAGS)

clean:
	rm -f miniv1d

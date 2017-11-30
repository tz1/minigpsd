CC=gcc

CFLAGS=$(shell pkg-config --cflags gconf-2.0)
LIBS=$(shell pkg-config --libs gconf-2.0)

#CFLAGS+= -g -Wall -I.
CFLAGS+= -O6 -I. -U_FORTIFY_SOURCE -Wall

all: mini6dof

mini6dof: mini6dof.c
	gcc -o $@ $^ $(LIBS) $(CFLAGS)

clean:
	rm -f mini6dof

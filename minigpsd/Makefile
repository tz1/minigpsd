CC=gcc

CFLAGS=$(shell pkg-config --cflags gconf-2.0)
LIBS=$(shell pkg-config --libs gconf-2.0)

#CFLAGS+= -g -Wall -I.
CFLAGS+= -O6 -I. -U_FORTIFY_SOURCE -Wall # -Wno-attributes

OBJS:= harley.o gpsdata.o minigpsd.o web.o

all: minigpsd kml2kmz

minigpsd: $(OBJS)
	gcc -o $@ $^ $(LIBS)

clean:
	rm -f minigpsd $(OBJS) kml2kmz

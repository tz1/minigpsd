
#CFLAGS+= -g -Wall -I../minigpsd
CFLAGS+= -O6 -I../minigpsd -U_FORTIFY_SOURCE -Wall # -Wno-attributes

all: harleyextract kmz2gpx mgpsd-via-mmap nmea2kml

nmea2kml: nmea2kml.c ../minigpsd/gpsdata.c

clean:
	rm -f harleyextract kmz2gpx mgpsd-via-mmap nmea2kml

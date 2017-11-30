//gcc -I minigpsd -o nmea2kml nmea2kml.c minigpsd/gpsdata.c minigpsd/globals.c

#include "minigpsd.h"
int gpsgatefd = -1;
int gpsgaterate = 1;
int thisms = 0;
int kmlinterval = 5;

struct gpsstate gpst;
struct gpssats gpsat;
FILE *logfd = NULL;
FILE *errfd = NULL;
char *zipkml = "/usr/bin/kml2kmz";

int main(int argc, char *argv[])
{
    char ibuf[4096];

    logfd = NULL;
    errfd = stderr;
    zipkml = "echo";
    kmlinterval=60;

    while (!feof(stdin)) {
	fgets(ibuf, 4095, stdin);
	getgpsinfo(ibuf);
    }
    sprintf(ibuf, "%02d%02d%02d%02d%02d.kml", gpst.yr, gpst.mo, gpst.dy, gpst.hr, gpst.mn);
    if (logfd) {
	fclose(logfd);
	rename("current.kml", ibuf);
    }

    return 0;			// quiet compiler
}

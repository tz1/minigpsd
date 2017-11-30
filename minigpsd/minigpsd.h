// Copyright 2008, tz
// released under GNU General Public License version 3.

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/un.h>

#define BUFLEN (16384)
#define MAXCONN (8)

#include "minigpsdmmap.h"

struct gpssats {
    int satset, nsats, nused;
    int sats[50], view[100]; // list of used, inview
    int el[100], az[100], sn[100];
};

struct harley {
    int rpm, vspd, full, gear, clutch, neutral, engtemp, turnsig, odoaccum, fuelaccum;
    int odolastms, fuellastms;
    int odolastval, fuellastval;
};

extern struct gpsstate gpst;
extern struct gpssats gpsat;

//commons
extern FILE *errfd;
extern FILE *logfd;
extern char *xbuf;

//configs
extern int kmlinterval;
extern char *zipkml;
extern char *rtname;

//FUNCTIONS
// process data from device
int getgpsinfo(char *);
void calcobd(char *, int);
void addnmeacksum(char *c);

//web subfunctions
void dogmap(void);
void dokml(char *);
void doweb(void);
void dorad(int);
void doxml(void);

//misc
void rotatekml(void);

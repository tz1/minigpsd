#include "minigpsd.h"

static char kmlname[80] = "";

//kml template

static char kmlhead[] =
  "<Document>\n"
  "<name>%s</name>\n"
  "<LookAt>"
  "<longitude>%d.%06d</longitude>\n"
  "<latitude>%d.%06d</latitude>\n"
  "<range>500</range>"
  "<tilt>60</tilt>"
  "<altitude>0</altitude>"
  "<altitudeMode>relativeToGround</altitudeMode>"
  "<heading>%d.%03d</heading>"
  "</LookAt>"
  "<Style id=\"x\"><LineStyle><width>2</width><color>ff0000ff</color></LineStyle>"
  "<PolyStyle><color>33000000</color></PolyStyle></Style>\n" "<Placemark><LineString><coordinates>\n";

static char pmarkfmt[] =
  "%d.%06d,%d.%06d,%d.%03d\n"
  "</coordinates></LineString></Placemark>\n"
  "<Placemark>\n"
  "<TimeStamp><when>20%02d-%02d-%02dT%02d:%02d:%02dZ</when></TimeStamp>\n"
  "<styleUrl>#x</styleUrl><LineString><extrude>1</extrude>" "<altitudeMode>relativeToGround</altitudeMode><coordinates>\n";

static char kmltail[] = "</coordinates></LineString></Placemark>\n</Document>\n";

//NMEA field data extraction helpers

static char *field[100];        // expanded to 100 for G-Rays PUBX03

void addnmeacksum(char *c)
{
    int i = 0;
    char *d = c;
    d += strlen(d);
    *d++ = '*';
    *d = 0;
    c++;
    while (*c && *c != '*')
        i ^= *c++;
    i &= 0xff;
    sprintf(++c, "%02X", i);
}

static int get2(char *c)
{
    int i = 0;
    if (*c)
        i = (*c++ - '0') * 10;
    if (*c)
        i += *c - '0';
    return i;
}

static int get3(char *c)
{
    int i = 0;
    if (*c)
        i = (*c++ - '0') * 100;
    i += get2(c);
    return i;
}

static int get3dp(int f)
{
    int i = 0;
    char *d = field[f];
    while (*d && *d != '.') {
        i *= 10;
        i += (*d++ - '0');
    }
    i *= 1000;
    if (*d == '.')
        d++;

    if (*d)
        i += (*d++ - '0') * 100;
    if (*d)
        i += (*d++ - '0') * 10;
    if (*d)
        i += *d++ - '0';
    return i;
}

static int get0dp(int f)
{
    int i = 0;
    char *d = field[f];
    while (*d && *d != '.') {
        i *= 10;
        i += (*d++ - '0');
    }
    return i;
}

static void gethms(int i)
{
    //hms field[i]
    char *c = field[i];
    gpst.hr = get2(c);
    gpst.mn = get2(&c[2]);
    gpst.sc = get2(&c[4]);
    if (c[6] && c[6] == '.')
        gpst.scth = get3(&c[7]);
}

static int getminutes(char *d)
{
    int i;
    i = (*d++ - '0') * 100000;
    //Minutes with decimal
    i += (*d++ - '0') * 10000;
    if (*d)
        d++;
    if (*d)
        i += (*d++ - '0') * 1000;
    if (*d)
        i += (*d++ - '0') * 100;
    if (*d)
        i += (*d++ - '0') * 10;
    if (*d)
        i += *d++ - '0';
    return i * 5 / 3;
}

static void getll(int f)
{
    int l, d;
    char *c;

    c = field[f++];
    l = get2(c);
    c += 2;
    d = getminutes(c);

    c = field[f++];
    l *= 1000000;
    l += d;
    if (*c != 'N')
        l = -l;
    //    if (l != gpst.llat)
    //        chg = 1;
    gpst.llat = l;

    c = field[f++];
    l = get3(c);
    c += 3;
    d = getminutes(c);

    c = field[f];

    l *= 1000000;
    l += d;
    if (*c != 'E')
        l = -l;
    //    if (l != gpst.llon)
    //        chg = 1;
    gpst.llon = l;

}

//KML Logging
static char kmlstr[BUFLEN];
static int kmlful = 0;

// append data, close KML, and rewind redy for next data
static void dokmltail()
{
    if (!logfd)
        return;
    strcpy(&kmlstr[kmlful], kmltail);
    fputs(kmlstr, logfd);
    kmlful = 0;
    kmlstr[0] = 0;
    fflush(logfd);
    fseek(logfd, -strlen(kmltail), SEEK_CUR);
}

void add2kml(char *add)
{
    strcpy(&kmlstr[kmlful], add);
    kmlful += strlen(add);
    if (kmlful > BUFLEN / 2)
        dokmltail();
}

// Save and zip current KML

static void kmzip(char *fname)
{
    fprintf(errfd, "exec: %s %s\n", zipkml, fname);
    if (!fork()) {
        execlp(zipkml, zipkml, fname, NULL);
        exit(-1);
    }
}

void rotatekml()
{
    char lbuf[256];
    struct timeval tv;
    struct tm *tmpt;

    // use syslock to avoid collisions and nonlocked time errors
    gettimeofday(&tv, NULL);
    tmpt = gmtime(&tv.tv_sec);

    sprintf(lbuf, "%02d%02d%02d%02d%02d%02d.kml",
      tmpt->tm_year % 100, 1 + tmpt->tm_mon, tmpt->tm_mday, tmpt->tm_hour, tmpt->tm_min, tmpt->tm_sec);

    // added gpst.mn++ to teardown - need to test
    if (!strcmp(lbuf, kmlname))
        lbuf[strlen(lbuf) - 5]++;       // increase minutes to avoid overwrite
    // normally only happens at end when sigint closes too quickly.
    strcpy(kmlname, lbuf);
    if (logfd) {
        dokmltail();            // write out anything remaining in buffer
        fclose(logfd);
        if (rename("current.kml", lbuf)) {      // no current.kml yet, starting
            lbuf[10] = 0;
            strcat(lbuf, ".lastcur.kml");
            rename("../prevcur.kml", lbuf);
            rename("prevcur.kml", lbuf);
            kmzip(lbuf);
            lbuf[10] = 0;
            strcat(lbuf, ".prlock.kml");
            //      if( ftell(logfd) <= strlen(kmltail) * 2 ) {} 
// if no data, don't rename prelock
            rename("../prlock.kml", lbuf);
            rename("prlock.kml", lbuf);
        }
        kmzip(lbuf);
    }
}

// sync internal lock on first lock
static int firslock = 0;
static void writelock()
{
    char cmd[256];
    int i;
    firslock = 1;

    // set system lock - linux generic
    // sprintf( cmd, "sudo date -u -s %02d/%02d/20%02d", gpst.mo,gpst.dy,gpst.yr );
    // sprintf( cmd, "sudo date -u -s %02d:%02d:%02d", gpst.hr,gpst.mn,gpst.sc );
    // sprintf( cmd, "sudo hwlock --systohc" );

    // nokia
    if (!access("/mnt/initfs/usr/bin/retutime", F_OK)) {
        sprintf(cmd, "sudo /usr/sbin/chroot /mnt/initfs /usr/bin/retutime -T 20%02d-%02d-%02d/%02d:%02d:%02d",
          gpst.yr, gpst.mo, gpst.dy, gpst.hr, gpst.mn, gpst.sc);
        i = system(cmd);
        //  fprintf( errfd, "Set Time %d=%s\n", i, cmd );
        system("sudo /usr/sbin/chroot /mnt/initfs /usr/bin/retutime -i");       // update system from RTC
    }
}

// process NMEA to set data

static int kmmn = -1;
static int kmlsc = -1;
static int kmscth = -1;

static char rmcbuf[132] = "";
static char ggabuf[132] = "";

extern int gpsgatefd;
extern char gpsgateimei[];
extern int gpsgaterate;
extern int gpsgatetcp;
static int gpsgatetime = 0;

// expects single null terminated strings (line ends dont matter)
int getgpsinfo(char *buf)
{
    char *c, *d;
    int i, fmax;

    c = buf;

    d = NULL;
    // required for pathologic cases of $GPABC...$GPXYZ...*ck 
    // where $GPABC... resolves to zero
    for (;;) {                  // find last $ - start of NMEA
        c = strchr(c, '$');
        if (!c)
            break;
        d = c;
        c++;
    }
    if (!d)
        return 0;

    // ignore all but standard NMEA
    if (strncmp(d, "$GP", 3))
        return 0;

    c = d;
    c++;

    //verify checksum
    i = 0;
    while (*c && *c != '*')
        i ^= *c++;
    if (!*c || (unsigned) (i & 0xff) != strtoul(++c, NULL, 16)) {
        fprintf(errfd, "Bad NMEA Checksum, calc'd %02x:\n %s", i, d);
        return -1;
    }
    --c;

    if (gpsgatefd > 0 && !strncmp(d, "$GPRMC", 6))
        strcpy(rmcbuf, d);
    if (gpsgatefd > 0 && !strncmp(d, "$GPGGA", 6))
        strcpy(ggabuf, d);

    *c = 0;
    //null out asterisk
    c = d;

    //Split into fields at the commas
    fmax = 0;
    c++;
    for (;;) {
        field[fmax++] = c;
        c = strchr(c, ',');
        if (c == NULL)
            break;
        *c++ = 0;
    }

    //Latitude, Longitude, and other info
    if (fmax == 13 && !strcmp(field[0], "GPRMC")) {
        //NEED TO VERIFY FMAX FOR EACH
        if (field[2][0] != 'A') {
            if( gpst.lock )
                gpst.lock = 0;
            return 1;
        } else {
            if( !gpst.lock )
                gpst.lock = 1;
            gethms(1);
            getll(3);
            gpst.gspd = get3dp(7) * 1151 / 1000;
            //convert to MPH
            gpst.gtrk = get3dp(8);
            //Date, DDMMYY
            gpst.dy = get2(field[9]);
            gpst.mo = get2(&field[9][2]);
            gpst.yr = get2(&field[9][4]);

            // this will be slightly late
            if (!firslock)
                writelock();
        }
    } else if (fmax == 15 && !strcmp(field[0], "GPGGA")) {
        i = field[6][0] - '0';
	// was gpst.lock, but it would prevent GPRMC alt
        if (!i)
            return 1;
        else
            if( gpst.lock != i )
                gpst.lock = i;
        // Redundant: getll(2);
        // don't get this here since it won't increment the YMD
        // and create a midnight bug
        //       gethms(1);
        //7 - 2 plc Sats Used
        // 8 - HDOP
        gpst.hdop = get3dp(8);
        gpst.alt = get3dp(9);
        //9, 10 - Alt, units M
    }
#if 0 // depend on RMC to avoid midnight bugs
    else if (fmax == 8 && !strcmp(field[0], "GPGLL")) {
        if (field[6][0] != 'A') {
#if 0       // this will cause problems for the kml rotate if the time is wrong
            if (strlen(field[5]))
                gethms(5);
#endif
           if( gpst.lock )
                gpst.lock = 0;
            return 1;
        }
        if( !gpst.lock )
            gpst.lock = 1;
        getll(1);
        gethms(5);
    }
#endif
#if 0
    else if (fmax == 10 && !strcmp(field[0], "GPVTG")) {
        gpst.gtrk = get3dp(1);
        gpst.gspd = get3dp(5) * 1151 / 1000;
        //convert to MPH
    }
#endif
    //Satellites and status
    else if (!(fmax & 3) && fmax >= 8 && fmax <= 20 && !strcmp(field[0], "GPGSV")) {
        int j, tot, seq, viewcnt;
        //should check (fmax % 4 == 3)
        tot = get0dp(1);
        seq = get0dp(2);
        viewcnt = 4 * (seq - 1);
        gpsat.nsats = get0dp(3);
        for (j = 4; j < 20 && j < fmax; j += 4) {
            i = get0dp(j);
            if (!i)
                return 1;
            gpsat.view[viewcnt++] = i;
            gpsat.el[i] = get0dp(j + 1);
            gpsat.az[i] = get0dp(j + 2);
            gpsat.sn[i] = get0dp(j + 3);
        }
        gpsat.satset &= (1 << tot) - 1;
        gpsat.satset &= ~ (1 << (seq-1));
	if( !gpsat.satset ) {
	    int n , m, k;
	    gpst.nsats = gpsat.nsats;
	    gpst.nused = gpsat.nused;
	    for (n = 0; n < gpsat.nsats; n++) {
	        m = gpsat.view[n];
		gpst.sats[n].num = m;
		gpst.sats[n].el = gpsat.el[m];
		gpst.sats[n].az = gpsat.az[m];
		gpst.sats[n].sn = gpsat.sn[m];
		for (k = 0; k < 12; k++)
		    if (gpsat.sats[k] == m)
		        break;
		if( k < 12 )
		    gpst.sats[n].num = -m;
	    }
	}
    } else if (fmax == 18 && !strcmp(field[0], "GPGSA")) {
        gpsat.satset = 255;
        gpst.fix = get0dp(2);
	gpsat.nused = 0;
        for (i = 3; i < 15; i++) {
            gpsat.sats[i] = get0dp(i);
	    if( gpsat.sats[i] )
	        gpsat.nused++;
	    // else break;?
	}
        gpst.pdop = get3dp(15);
        gpst.hdop = get3dp(16);
        gpst.vdop = get3dp(17);
    }
#if 0
    else
        printf("%s\n", field[0]);
#endif

    if (!gpst.mo || !gpst.dy)
        return 1;
    // within 24 hours, only when gpst.lock since two unlocked GPS can have different times
    if (kmlinterval && gpst.lock && kmmn != (gpst.hr * 60 + gpst.mn) / kmlinterval) {
        kmmn = (gpst.hr * 60 + gpst.mn) / kmlinterval;
        rotatekml();
        logfd = fopen("current.kml", "w+b");
	if( logfd ) {
        fprintf(logfd, kmlhead, kmlname, gpst.llon / 1000000, abs(gpst.llon % 1000000), 
                gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.gtrk / 1000,
          gpst.gtrk % 1000);
        fflush(logfd);
	}
    }
    if (kmlsc == gpst.sc && kmscth == gpst.scth)
        return 1;
    if (!gpst.llat && !gpst.llon)         // time, but no location
        return 1;

    if (kmlsc != gpst.sc) {
        if (gpsgatefd > 0 && --gpsgatetime <= 0 ) {
            gpsgatetime = gpsgaterate;

            if( !gpsgatetcp ) // relogin
                i = write(gpsgatefd, gpsgateimei, strlen(gpsgateimei));
            i = 1;
            if (rmcbuf[0] != 0) {
                i = write(gpsgatefd, rmcbuf, strlen(rmcbuf));
                rmcbuf[0] = 0;
            }
            if (ggabuf[0] != 0) {
                i = write(gpsgatefd, ggabuf, strlen(ggabuf));
                ggabuf[0] = 0;
            }
            if (i <= 0) {
                close(gpsgatefd);
                gpsgatefd = -1;
            }
        }
	if( !kmlinterval || !logfd )
		return 1;
        char lux[32],temp[32], pdsns[80];
        FILE *fp;
        extern int thisms;

        kmlsc = gpst.sc;

        //#ifdef NOKIATABLET ?
        // Sensors
        lux[0] = 0;
        temp[0] = 0;
        // should really see if rewind works
        if( (fp = fopen( "/sys/devices/platform/i2c_omap.1/i2c-1/1-0048/temp1_input", "r" ) )) {
            fgets( temp, 32, fp );
            fclose( fp );
        }
        if( (fp = fopen( "/sys/devices/platform/i2c_omap.2/i2c-0/0-0029/lux", "r" ) )) {
            fgets( lux, 32, fp );
            fclose( fp );
        }
	if( strlen(lux) || strlen(temp) ) {
            sprintf(pdsns, "$PNSNS,%d,%d", atoi(lux), atoi(temp) );
            addnmeacksum(pdsns);
            sprintf(&kmlstr[kmlful], "<!--%05dt( %s )-->\n", thisms, pdsns );
            kmlful = strlen(kmlstr);
	}

        //sprint then fputs in dokmltail to make it a unitary write
        //(otherwise current.kml may be read as a partial)
        sprintf(&kmlstr[kmlful], pmarkfmt, gpst.llon / 1000000, abs(gpst.llon % 1000000),
                gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.gspd / 1000, gpst.gspd % 1000,  // first and last
          gpst.yr, gpst.mo, gpst.dy, gpst.hr, gpst.mn, gpst.sc);
        kmlful = strlen(kmlstr);
    }

    kmscth = gpst.scth;
    sprintf(&kmlstr[kmlful], "%d.%06d,%d.%06d,%d.%03d\n", gpst.llon / 1000000, abs(gpst.llon % 1000000), 
            gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.gspd / 1000, gpst.gspd % 1000);
    kmlful = strlen(kmlstr);

    if (kmlful > BUFLEN / 2)
        dokmltail();
    return 2;
}

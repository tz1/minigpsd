#include "minigpsd.h"

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


void getgpsinfo(char *buf)
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
        return;

    // ignore all but standard NMEA
    if (strncmp(d, "$GP", 3))
        return;

    c = d;
    c++;

    //verify checksum
    i = 0;
    while (*c && *c != '*')
        i ^= *c++;
    if (!*c || (unsigned) (i & 0xff) != strtoul(++c, NULL, 16)) {
        fprintf(stderr, "Bad NMEA Checksum, calc'd %02x:\n %s", i, d);
        return;
    }
    --c;

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
            if (strlen(field[1]))
                gethms(1);
            if (strlen(field[9])) {
                gpst.dy = get2(field[9]);
                gpst.mo = get2(&field[9][2]);
                gpst.yr = get2(&field[9][4]);
            }
            gpst.lock = 0;
            return;
        } else {
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
            if (!firslock && !gpst.scth)
                writelock();
        }
    } else if (fmax == 15 && !strcmp(field[0], "GPGGA")) {
        gpst.lock = field[6][0] - '0';
        if (!gpst.lock)
            return;
        getll(2);
        gethms(1);
        //7 - 2 plc Sats Used
        // 8 - HDOP
        gpst.hdop = get3dp(8);
        gpst.alt = get3dp(9);
        //9, 10 - Alt, units M
    } else if (fmax == 8 && !strcmp(field[0], "GPGLL")) {
        if (field[6][0] != 'A') {
#if 0                           // this will cause problems for the kml rotate if the time is wrong
            if (strlen(field[5]))
                gethms(5);
#endif
            gpst.lock = 0;
            return;
        }
        gpst.lock = 1;
        getll(1);
        gethms(5);
    }
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
                return;
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
}

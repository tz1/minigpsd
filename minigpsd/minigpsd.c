#include "minigpsd.h"
#include <errno.h>
#include <sys/termios.h>

struct gpsstate gpst;
struct gpssats gpsat;
struct harley hstat = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};

struct moredata {
    unsigned int v1img;
    unsigned int v1flash;
    unsigned int xa,ya,za;
};

struct moredata more = {0,0,0,0,0};

FILE *errfd = NULL;
FILE *logfd = NULL;
char *xbuf;

//config params
int kmlinterval = 5;
char *zipkml = "/usr/bin/kml2kmz";
char *rtname = "Mini GPSD";

void fixserio(int fd)
{
    struct termios termst;
    if (fd < 0)
        return;
    if (-1 == tcgetattr(fd, &termst))
        return;

    cfmakeraw(&termst);

    termst.c_iflag |= IGNCR;
    termst.c_lflag |= ICANON;

    tcsetattr(fd, TCSANOW, &termst);
    tcflush(fd, TCIOFLUSH);
}

extern void add2kml(char *);

static int acpt[MAXCONN];
static int raw[MAXCONN];
static int amax = 0;

//need conn to toggle raw for it
static void prtgpsinfo(int conn, char *c)
{
    char cbuf[256];
    int n;
    struct timeval tv;
    strcpy(xbuf, "GPSD");
    while (*c) {
        if (*c >= 'a')
            *c -= 32;
        switch (*c) {
            // bauds were here
        case 'P':
            sprintf(cbuf, ",P=%d.%06d %d.%06d", gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.llon / 1000000, abs(gpst.llon % 1000000));
            break;
        case 'A':
            sprintf(cbuf, ",A=%d.%03d", gpst.alt / 1000, abs(gpst.alt % 1000));
            break;
        case 'D':
            sprintf(cbuf, ",D=%02d-%02d-%02d %02d:%02d:%02d", gpst.yr, gpst.mo, gpst.dy, gpst.hr, gpst.mn, gpst.sc);
            break;
        case 'V':
            sprintf(cbuf, ",V=%d.%03d", gpst.gspd / 1000, gpst.gspd % 1000);
            break;
        case 'S':
            sprintf(cbuf, ",S=%d", !!gpst.lock);
            break;
        case 'M':
            sprintf(cbuf, ",M=%d", gpst.fix);
            break;
        case 'O':
            gettimeofday(&tv, NULL);
            sprintf(cbuf, ",O=- %d.%02d ? %d.%06d %d.%06d %d.%03d %d.%02d %d.%02d %d.%03d %d.%03d ? ? ? %d",
              (unsigned int) tv.tv_sec, (unsigned int) tv.tv_usec / 10000,
              gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.llon / 1000000, abs(gpst.llon % 1000000),
              gpst.alt / 1000, abs(gpst.alt % 1000),
              gpst.hdop / 1000, gpst.hdop % 1000 / 10,
              gpst.vdop / 1000, gpst.vdop % 1000 / 10, gpst.gtrk / 1000, gpst.gtrk % 1000, gpst.gspd / 1000, gpst.gspd % 1000, gpst.fix);
            break;
        case 'R':
            raw[conn] = !raw[conn];
            sprintf(cbuf, ",R=%d", raw[conn]);
            break;
        case 'E':
            sprintf(cbuf, ",E=%d.%02d %d.%02d %d.%02d", gpst.pdop / 1000, gpst.pdop % 1000 / 10, gpst.hdop / 1000, gpst.hdop % 1000 / 10,
              gpst.vdop / 1000, gpst.vdop % 1000 / 10);
            break;
        case 'H':
            sprintf(cbuf, ",H=%d.%03d", gpst.gtrk / 1000, gpst.gtrk % 1000);
            break;
        case 'J':
            strcat(xbuf, "\r\nSN EL AZM SG U");
            for (n = 0; n < gpst.nsats; n++) {
                sprintf(cbuf, "\r\n%d %02d %02d %03d %02d", gpst.sats[n].num < 0, abs(gpst.sats[n].num), gpst.sats[n].el, gpst.sats[n].az, gpst.sats[n].sn);
                strcat(xbuf, cbuf);
            }
            cbuf[0] = 0;
            break;
        case 'Y':
            gettimeofday(&tv, NULL);
            sprintf(xbuf, "GPSD,Y=- %d.%06d %d:", (unsigned int) tv.tv_sec, (unsigned int) tv.tv_usec, gpst.nsats);
            for (n = 0; n < gpst.nsats; n++) {
                sprintf(cbuf, "%d %d %d %d %d:", abs(gpst.sats[n].num), gpst.sats[n].el, gpst.sats[n].az, gpst.sats[n].sn, gpst.sats[n].num < 0);
                strcat(xbuf, cbuf);
                cbuf[0] = 0;
            }
            break;
        case 'T':
            sprintf(cbuf, ",Alt Date E-phv-dop Hdng Mode Pos Raw Status This Velo Ysats Ovvu Jsats");
            break;
        default:
            cbuf[0] = 0;
            break;
        }
        c++;
        strcat(xbuf, cbuf);
    }
    strcat(xbuf, "\r\n");
    //should be "\r\n" for network
    n = strlen(xbuf);
    if (n != write(acpt[conn], xbuf, n)) {
        close(acpt[conn]);
        acpt[conn] = -1;
    }
}

static int igpsfd = -1;

static void sendnokgps(char *cmd)
{
    int n, len;
    struct sockaddr_un igps;
    n = socket(AF_UNIX, SOCK_STREAM, 0);
    if (n >= 0) {
        igps.sun_family = AF_UNIX;
        strcpy(igps.sun_path, "/var/lib/gps/gps_driver_ctrl");
        len = strlen(igps.sun_path) + sizeof(igps.sun_family);
        if (connect(n, (struct sockaddr *) &igps, len) != -1) {
            char buf[4];
            if (!fork()) {      // this can take a while
                write(n, cmd, strlen(cmd));
                read(n, buf, 4);
                close(n);
                exit(0);
            }
            close(n);
        }
    }
}

static void doraw(char *str)
{
    int i;
    for (i = 0; i < amax; i++) {
        if (acpt[i] == -1)
            continue;
        if (!raw[i])
            continue;
        if (strlen(str) != write(acpt[i], str, strlen(str))) {
            close(acpt[i]);
            acpt[i] = -1;
        }
    }
}

static char temppath[256];
static char *igpspath = "/dev/pgps";
static char *gpspath = NULL;
static char *obdpath = NULL;

static int gpspid = -1, obdpid = -1;
static char gpsbtaddr[50] = "", obdbtaddr[50] = "";

static char *unixsock = "/tmp/.gpsd_ctrl_sock";
static char *pidlockfile = "/tmp/minigpsd.pid";

#include <gconf/gconf-client.h>
const char *mypath = "/apps/maemo/minigpsd/";

static int stafd = -1;
static void teardown(int signo)
{
    fprintf(errfd, "Shutdown\n");
    signal(SIGCHLD, SIG_IGN);
    fflush(errfd);
    gpst.mn++;
    rotatekml();

    fclose(errfd);
    if (igpsfd >= 0) {
        if (igpspath && !strcmp(igpspath, "/dev/pgps"))
            sendnokgps("P 0\n");        // shutdown internal
        close(igpsfd);
    }

    if (gpspid > 0)
        kill(gpspid, SIGINT);
    if (obdpid > 0)
        kill(obdpid, SIGINT);
    if (gpst.obdfd >= 0)
        close(gpst.obdfd);
    if (gpst.gpsfd >= 0)
        close(gpst.gpsfd);
    sleep(2);
    if (gpspid > 0)
        kill(gpspid, SIGKILL);
    if (obdpid > 0)
        kill(obdpid, SIGKILL);

    unlink(unixsock);
    unlink(pidlockfile);

    memset( &gpst, 0, sizeof(gpst) );
    gpst.gpsfd = -1;
    gpst.obdfd = -1;

    lseek(stafd, 0, SEEK_SET);
    write(stafd, &gpst, sizeof(gpst) );
    close(stafd);

#if 1
    // Set agps latlon - is read, so will simply set to startup values
    if (gpst.llat && gpst.llon) {
        GConfClient *clint = NULL;
        double d;
        char c[100];

        g_type_init();
        clint = gconf_client_get_default();
        strcpy(c, "/system/osso/supl/");
        strcat(c, "pos_latitude");
        d = gpst.llat;
        d /= 1000000.0;
        gconf_client_set_float(clint, c, d, NULL);

        strcpy(c, "/system/osso/supl/");
        strcat(c, "pos_longitude");
        d = gpst.llon;
        d /= 1000000.0;
        gconf_client_set_float(clint, c, d, NULL);
    }
#endif

    exit(0);
}

static char gpsrfc[20] = "rfcomm0";
static char obdrfc[20] = "rfcomm1";

#include <sys/wait.h>

static void reconn(int signo)
{
    int status = 0, child = -1, i;
    //    char cmd[128];

    do {
        child = waitpid(-1, &status, WNOHANG);
        fprintf(errfd, "Child %d exited with status %d\n", child, status);
        if( child > 0 ) {// for startup subprocess hiccups - nohang will 
            if( child == gpspid ){
                gpspid = -1;
                if (!(gpspid = fork())) {
                    for( i = 3; i < 1024; i++ )
                        close(i);
                    execlp("sh", "sh", "/usr/bin/btconnect.sh", gpsrfc, gpsbtaddr, NULL);
                    exit(-1);
                }
            }
            else if( child == obdpid ){
                obdpid = -1;
                if (!(obdpid = fork())) {
                    for( i = 3; i < 1024; i++ )
                        close(i);
                    execlp("sh", "sh", "/usr/bin/btconnect.sh", obdrfc, obdbtaddr, NULL);
                    exit(-1);
                }
            }
        }
    } while (child > 0 ); // end when no more child processes exit
    signal(SIGCHLD, reconn);
    // rest are kml zips
}

static void checkgconf(char *key, char *val)
{
    char c[256];
    const char *c1 = NULL;
    GConfClient *clint = NULL;
    GConfValue *gcv = NULL;

    *val = 0;
    g_type_init();
    clint = gconf_client_get_default();
    strcpy(c, mypath);
    strcat(c, key);
    gcv = gconf_client_get_without_default(clint, c, NULL);
    if (gcv) {
        c1 = gconf_value_get_string(gcv);
        if (c1)
            strcpy(val, c1);
        gconf_value_free(gcv);
    }
    g_object_unref(clint);
    //    fprintf(stderr, "%s [%s]=[%s]\n", mypath, key, val);
}

static void getagpslatlon() {
    GConfClient *clint = NULL;
    double d;
    char c[100];
    GConfValue *gcv = NULL;

    g_type_init();

    strcpy(c, "/system/osso/supl/");
    strcat(c, "pos_latitude");
    clint = gconf_client_get_default();
    gcv = gconf_client_get_without_default(clint, c, NULL);
    d = 0.0;
    if (gcv) {
        d = gconf_value_get_float(gcv);
        gconf_value_free(gcv);
    }
    g_object_unref(clint);
    gpst.llat = (int)( d * 1000000.0 );

    strcpy(c, "/system/osso/supl/");
    strcat(c, "pos_longitude");
    clint = gconf_client_get_default();
    gcv = gconf_client_get_without_default(clint, c, NULL);
    d = 0.0;
    if (gcv) {
        d = gconf_value_get_float(gcv);
        gconf_value_free(gcv);
    }
    g_object_unref(clint);
    gpst.llon = (int)( d * 1000000.0 );
}

static int gpsdport = 2947;
static int gpsthru = 22947;
static int gpsanno = 32947;
static int obdthru = 22534;
static int httpport = 8888;
static char *logdirprefix = "/tmp/";
char *gpsgateurl = NULL;
char gpsgateimei[64] = "";
int gpsgatefd = -1;
static int gpsannofd = -1;
int gpsgatetcp = 0;
int iconupdaterate = 3;
int gpsgaterate = 1;

#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>

void dogpsgate()
{
    char login[256];
    int n, port = 30175, temp;
    struct sockaddr_in sin;
    char *sport = NULL;
    char *imei = NULL;

    if (!gpsgateurl)
        return;

    strncpy(login, gpsgateurl, 250);

    imei = strstr(login, "/");
    if (!imei)
        return;
    *imei++ = 0;
    if (!strlen(imei))
        return;

    sport = strstr(login, ":");
    if (sport) {
        *sport++ = 0;
        n = atoi(sport);
        if (n > 0)
            port = n;
    }

    memset((char *) &sin, 0, sizeof(sin));
    temp = inet_addr(login);
    if (temp != INADDR_NONE)
        memcpy(&sin.sin_addr, &temp, sizeof(temp));
    else {
        struct hostent *host = NULL;
        host = gethostbyname(login);
        if (host == NULL)
            return;
        memcpy((char *) &sin.sin_addr, host->h_addr, host->h_length);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (gpsgatetcp)
        gpsgatefd = socket(AF_INET, SOCK_STREAM, 0);
    else
        gpsgatefd = socket(AF_INET, SOCK_DGRAM, 0);

    n = connect(gpsgatefd, (struct sockaddr *) &sin, sizeof(sin));
    if (n < 0) {
        close(gpsgatefd);
        gpsgatefd = -1;
        return;
    }

    sprintf(gpsgateimei, "$FRLIN,IMEI,%s,", imei);
    addnmeacksum(gpsgateimei);
    strcat(gpsgateimei, "\r\n");

    n = write(gpsgatefd, gpsgateimei, strlen(gpsgateimei));
    if (n > 0) {
        if (gpsgatetcp)
            n = read(gpsgatefd, login, strlen(login));
        //errfd
        //fprintf(stderr, "gpsgate: %d %s\n", n, login);
        return;
    }
    fprintf(stderr, "gpsgate disconnect\n");
    close(gpsgatefd);
    gpsgatefd = -1;
}

static void getconfig()
{
    int tempport;

    getagpslatlon();
    // atoi
    checkgconf("KMLINTERVAL", temppath);        // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        kmlinterval = tempport;
    }
    checkgconf("GPSDPORT", temppath);   // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        if (tempport > 0)
            gpsdport = tempport;
    }
    checkgconf("GPSTHRU", temppath);    // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        if (tempport > 0)
            gpsthru = tempport;
    }
    checkgconf("GPSANNO", temppath);    // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        if (tempport > 0)
            gpsanno = tempport;
    }
    checkgconf("HTTPPORT", temppath);   // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        if (tempport > 0)
            httpport = tempport;
    }
    checkgconf("ICONUPDATE", temppath); // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        if (tempport > 0)
            iconupdaterate = tempport;
    }
    // string malloc
    checkgconf("ZIPKML", temppath);
    if (strlen(temppath)) {
        zipkml = malloc(strlen(temppath) + 1);
        strcpy(zipkml, temppath);
    }

    checkgconf("LOGDIR", temppath);
    if (strlen(temppath)) {
        if( !strncmp( temppath, "/media/mmc", 10 ) ) {
	    char s[80];
	    sprintf( s, "cat /proc/mounts | grep %11.11s", temppath );
	    if( system( s ) )
	        strcpy(temppath, "/not/a/valid/directory/");
	}
      logdirprefix = malloc(strlen(temppath) + 1);
      strcpy(logdirprefix, temppath);
    }
    mkdir(logdirprefix, 0777);
    if (chdir(logdirprefix)) {
        system( "ossonotify 'GPSD: No Folder, Logging Disabled'" );
        printf( "GPSD: No Folder, Logging Disabled" );
        chdir("/tmp");
        logdirprefix =  "/tmp/";
	kmlinterval = 0;
    }
    checkgconf("UseGPSGate", temppath);
    if (strlen(temppath) && atoi(temppath) == 1) {
        checkgconf("GPSGATErate", temppath);
        gpsgaterate = atoi(temppath);
        gpsgateurl = malloc(1024);
        checkgconf("GPSGATEhost", temppath);
        strcpy(gpsgateurl, temppath);
        checkgconf("GPSGATEport", temppath);
        strcat(gpsgateurl, ":");
        strcat(gpsgateurl, temppath);
        strcat(gpsgateurl, "/");
        checkgconf("GPSGATEimei", temppath);
        strcat(gpsgateurl, temppath);
        checkgconf("GPSGATEtcp", temppath);
        if (strlen(temppath))
            gpsgatetcp = (atoi(temppath) == 1);
    }

    checkgconf("UNIXSOCK", temppath);   // malloc
    if (strlen(temppath)) {
        unixsock = malloc(strlen(temppath) + 1);
        strcpy(unixsock, temppath);
    }
    checkgconf("MYNAME", temppath);     // malloc
    if (strlen(temppath)) {
        rtname = malloc(strlen(temppath) + 1);
        strcpy(rtname, temppath);
    }
    // path malloc
    checkgconf("GPSI", temppath);
    // default to Internal, path or OFF
    if (strlen(temppath)) {
        if (!strcmp(temppath, "OFF"))
            igpspath = NULL;
        else if (!strcmp(temppath, "Internal"));
        else if (!strcmp(temppath, "ON"));
        else if (!access(temppath, F_OK)) {
            igpspath = malloc(strlen(temppath) + 1);
            strcpy(igpspath, temppath);
        }
        // default to /dev/pgps
    }

    checkgconf("USEGPS", temppath);
    if ((atoi(temppath) == 1)) {
        checkgconf("GPSdev", temppath);
        if (strlen(temppath)) {
            if (!strncmp(temppath, "/dev/rfcomm", 11)) {
                gpspath = malloc(strlen(temppath) + 1);
                strcpy(gpspath, temppath);
                strcpy(gpsrfc, &temppath[11]);
                checkgconf("GPSaddr", temppath);
                if (strlen(temppath)) {
                    strcpy(gpsbtaddr, temppath);
                    if (!(gpspid = fork())) {
                        execlp("sh", "sh", "/usr/bin/btconnect.sh", gpsrfc, gpsbtaddr, NULL);
                        exit(-1);
                    }
                }
            } else {
                gpspath = malloc(strlen(temppath) + 1);
                strcpy(gpspath, temppath);
            }
        }
    }
    checkgconf("USEHD", temppath);
    if ((atoi(temppath) == 1)) {
        checkgconf("OBDdev", temppath);
        if (strlen(temppath)) {
            if (!strncmp(temppath, "/dev/rfcomm", 11)) {
                obdpath = malloc(strlen(temppath) + 1);
                strcpy(obdpath, temppath);
                strcpy(obdrfc, &temppath[11]);
                checkgconf("OBDaddr", temppath);
                if (strlen(temppath)) {
                    strcpy(obdbtaddr, temppath);
                    if (!(obdpid = fork())) {
                        execlp("sh", "sh", "/usr/bin/btconnect.sh", obdrfc, obdbtaddr, NULL);
                        exit(-1);
                    }
                }
            } else {
                obdpath = malloc(strlen(temppath) + 1);
                strcpy(obdpath, temppath);
            }
            // only do OBDTHRU if enabled
            checkgconf("OBDTHRU", temppath);    // atoi
            if (strlen(temppath)) {
                tempport = atoi(temppath);
                if (tempport > 0)
                    obdthru = tempport;
            }
        }
    }

    if (gpsgateurl)             // && gpsgatefd < 0 ) // for reconn
        dogpsgate();

    if (igpspath && !strcmp(igpspath, "/dev/pgps"))
        sendnokgps("P 3\n");    // start internal gps

    if (igpspath && !access(igpspath, F_OK))
        igpsfd = open(igpspath, O_RDONLY | O_NONBLOCK);
    if (gpspath && !access(gpspath, F_OK))
        gpst.gpsfd = open(gpspath, O_RDONLY | O_NONBLOCK);
    if (obdpath && !access(obdpath, F_OK))
        gpst.obdfd = open(obdpath, O_RDONLY | O_NONBLOCK);

    fixserio(igpsfd);
    fixserio(gpst.gpsfd);
    fixserio(gpst.obdfd);
}

static int pilock(void)
{
    int fd;
    int pid;
    char mypid[50];
    char pidstr[32];
    FILE *pidlockfp;

    pidlockfp = fopen(pidlockfile, "r");
    if (pidlockfp != NULL) {
        fgets(pidstr, 30, pidlockfp);
        pid = atoi(pidstr);
        fclose(pidlockfp);
        if (pid <= 0 || kill(pid, 0) == -1)       //stale?
            unlink(pidlockfile);
        else
            return 1;                   // active
    }
    sprintf(mypid, "%d\n", getpid());
    fd = open(pidlockfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0)
        return 1;
    write(fd, mypid, strlen(mypid));
    close(fd);
    return 0;
}

static void dobindgpsanno()
{
    int n;
    struct sockaddr_in sin;

    gpsannofd = socket(AF_INET, SOCK_DGRAM, 0);
    if( gpsannofd < 0 )
        return; //exit?

    memset((char *) &sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl( INADDR_ANY );
    sin.sin_family = AF_INET;
    sin.sin_port = htons(gpsanno);    
    n = bind(gpsannofd, (struct sockaddr *) &sin, sizeof(sin));
    if (n < 0) {
        close(gpsannofd);
        gpsannofd = -1;
        return;
    }
}

static void dobindlstn(int *sock, int port)
{
    struct sockaddr_in sin;
    int n;
    memset((char *) &sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    n = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(int));
    if (0 > bind(*sock, (struct sockaddr *) &sin, sizeof(sin)))
        exit(-1);
}

static void initfromlock()
{
    char sessdir[256];
    struct timeval tv;
    struct tm *tmpt;

    gettimeofday(&tv, NULL);
    tmpt = gmtime(&tv.tv_sec);
    gpst.scth = 1;
    gpst.sc = tmpt->tm_sec;
    gpst.mn = tmpt->tm_min;
    gpst.hr = tmpt->tm_hour;
    gpst.dy = tmpt->tm_mday;
    gpst.mo = 1 + tmpt->tm_mon;
    gpst.yr = tmpt->tm_year % 100;

    sprintf(sessdir, "%02d%02d%02d%02d%02d%02d", gpst.yr, gpst.mo, gpst.dy, gpst.hr, gpst.mn, gpst.sc);

    // create timestamped directory
    mkdir(sessdir, 0777);
    chdir(sessdir);
}

int thisms = 0;
static int getms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    thisms = tv.tv_usec / 1000 + 1000 * (tv.tv_sec % 100);
    return thisms;
}

static int lasmn = -1;
int main(int argc, char *argv[])
{
    int n, i;
    int lstn = -1, thru = -1, lthru = -1, lweb = -1, lmax = 0, lobdn = -1, obdn = -1, lunix = -1;
    unsigned int u;
    fd_set fds, fds2, lfds;
    int gfd, greop;
    struct sockaddr_in sin;
    struct sockaddr_un sun;
    char buf[512];
    struct timeval tv;
    time_t lastcheck;
    unsigned int ll, llprev=0, llpvpv[3] = {0,0,0};
    unsigned int mainlock = 0;

    if (!geteuid()) {
        fprintf(stderr, "Don't run as root!\n");
        exit(-2);
    }

    if (pilock())
        exit(-1);

    memset( &gpst, 0, sizeof(gpst) );
    gpst.gpsfd = -1;
    gpst.obdfd = -1;

    errfd = fopen("/tmp/gpsd.log", "w");
    fprintf(errfd, "Startup\n");
    fflush(errfd);

    signal(SIGCHLD, reconn);
    getconfig();

    for (n = 0; n < MAXCONN; n++)
        acpt[n] = -1;

    memset((char *) &sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    dobindgpsanno();
    dobindlstn(&lstn, gpsdport);
    dobindlstn(&lthru, gpsthru);
    dobindlstn(&lobdn, obdthru);
    dobindlstn(&lweb, httpport);

    lunix = socket(AF_UNIX, SOCK_STREAM, 0);
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, unixsock);
    unlink(unixsock);
    if (bind(lunix, (struct sockaddr *) &sun, (int) sizeof(sun)))
        fprintf(errfd, "bind on unix sock failed\n");
    else if (listen(lunix, 3))
        fprintf(errfd, "listen on unix sock failed\n");

    initfromlock();

    xbuf = malloc(BUFLEN);

    //place for OBD data before time and GPS gpst.lock for position
    if (kmlinterval)
        logfd = fopen("prlock.kml", "a+b");
    add2kml("<Document><name>Pre-Lock</name><Placemark><LineString><coordinates>0,0,0\n");

    signal(SIGTERM, teardown);
    signal(SIGINT, teardown);
    signal(SIGQUIT, teardown);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    listen(lstn, 3);
    listen(lthru, 1);
    listen(lobdn, 1);
    listen(lweb, 3);
    lmax = lstn;

    if (lthru > lmax)
        lmax = lthru;
    if (lobdn > lmax)
        lmax = obdn;
    if (lweb > lmax)
        lmax = lweb;
    if (lunix > lmax)
        lmax = lunix;
    if (gpsannofd > lmax)
        lmax = gpsannofd;

    FD_ZERO(&lfds);
    FD_SET(lthru, &lfds);
    FD_SET(lobdn, &lfds);
    FD_SET(lweb, &lfds);
    FD_SET(lstn, &lfds);
    FD_SET(lunix, &lfds);
    FD_SET(gpsannofd, &lfds);

    gettimeofday(&tv, NULL);
    lastcheck = tv.tv_sec;

    stafd = open("/tmp/mgpstate", O_RDWR | O_CREAT , 0644);
    write(stafd, &gpst, sizeof(gpst) );
    write(stafd, &hstat, sizeof(hstat) );
    write(stafd, &more, sizeof(more) );

    for (;;) {
        gettimeofday(&tv, NULL);
        // check for reopen every few seconds - e.g. BT reconnect or USB insert
        if (tv.tv_sec - lastcheck > 5) {
            if (igpspath && igpsfd < 0 && !access(igpspath, F_OK)) {
                igpsfd = open(igpspath, O_RDONLY | O_NONBLOCK);
                fprintf(errfd, "open %s %d\n", igpspath, igpsfd);
                fixserio(igpsfd);
            }
            if (obdpath && gpst.obdfd < 0 && !access(obdpath, F_OK)) {
                gpst.obdfd = open(obdpath, O_RDONLY | O_NONBLOCK);
                fprintf(errfd, "open %s %d\n", obdpath, gpst.obdfd);
                fixserio(gpst.obdfd);
            }
            if (gpspath && gpst.gpsfd < 0 && !access(gpspath, F_OK)) {
                gpst.gpsfd = open(gpspath, O_RDONLY | O_NONBLOCK);
                fprintf(errfd, "open %s %d\n", gpspath, gpst.gpsfd);
                fixserio(gpst.gpsfd);
            }
            lastcheck = tv.tv_sec;
        }

        fds = lfds;
        n = lmax;
#define MAXFD(fd) if (fd >= 0) { FD_SET(fd, &fds); if (fd > n) n = fd; }
        MAXFD(igpsfd);
        MAXFD(gpst.gpsfd);
        MAXFD(gpst.obdfd);
        MAXFD(thru);
        MAXFD(obdn);

        for (i = 0; i < amax; i++)
            MAXFD(acpt[i]);

        tv.tv_sec = 0;
        tv.tv_usec = 0;
        fds2 = fds;
        i = select(++n, &fds, NULL, NULL, &tv);
        if (!i) {               // no activity
	    lseek(stafd, 0, SEEK_SET);
	    write(stafd, &gpst, sizeof(gpst) );
	    write(stafd, &hstat, sizeof(hstat) );
	    write(stafd, &more, sizeof(more) );

	    // gpsgate restart
	    if (gpsgateurl && gpsgatefd < 0 && lasmn != gpst.mn) {
	      lasmn = gpst.mn;
	      dogpsgate();
	    }

            tv.tv_sec = 5;
            tv.tv_usec = 0;
            i = select(n, &fds2, NULL, NULL, &tv);
        }
        if (i < 0)
            continue;           // if BT device detaches...

        // timeout added to reconnect
        if (i == 0)
            continue;

        getms();                // timestamp this set - some will be later, but precision doesn't help

        if (gpsannofd >= 0 && FD_ISSET(gpsannofd, &fds)) {
            char tbuf[600];
            n = read( gpsannofd, buf, 512 );
            if( n < 0 )
                gpsannofd = -1;

            if (logfd) {
                buf[n] = 0;
#if 1
                for( i = 0 ; i < n ; i++ )
                    if( buf[i] == '\t' )
                        buf[i] = ',';
                    else if( buf[i] < ' ' ) {
                        buf[i] = '0';
                        break;
                    }
#endif
                // need to remove any -- from string?
                if( strncmp( "$P", buf, 2 ) ) {
                    sprintf(tbuf, "$PDLOG,%s", buf);
                    addnmeacksum(tbuf);
                    sprintf(xbuf, "<!--%05dl( %s )-->\n", thisms, tbuf);
                }
                else
                    sprintf(xbuf, "<!--%05dl( %s )-->\n", thisms, buf);
                add2kml(xbuf);
            }
        }

        // process serial AUX data
        if (gpst.obdfd >= 0 && FD_ISSET(gpst.obdfd, &fds)) {
            for (;;) {
                n = read(gpst.obdfd, buf, 500);
                if (n == -1 && errno == EAGAIN)
                    break;
                if (n <= 0) {
                    close(gpst.obdfd);
                    gpst.obdfd = -1;
                    fprintf(errfd, "obdfd err %d\n", n);
                    fflush(errfd);
                    break;
                }

                if( n > 0 && buf[0] == 'J' ) {
		    buf[n] = 0;
                    calcobd(buf, thisms);
                    sprintf(xbuf, "<!--%05dg( %s )-->\n", thisms, buf);
                    add2kml(xbuf); //decoded version
		    continue;
                }


                if (n > 0) {
                    // remove trailing whitespace and linefeed
                    while (n && buf[n - 1] <= ' ')
                        n--;
                    // Clean out non-ascii (comm hiccups) which make invalid XML
                    // for obd, could limit to space, 0-9, A-Z
                    for (i = 0; i < n; i++)
                        if (buf[i] < ' ' || buf[i] > 0x7e)
                            buf[i] = '.';
                    buf[n] = 0;
                }

                if (strlen(buf) > 0) {
                    if (obdn != -1) {
                        buf[n] = '\n';
                        buf[n + 1] = 0;
                        if (n != write(obdn, buf, n)) {
                            close(obdn);
                            obdn = -1;
                        }
                        buf[n] = 0;
                    }

                    calcobd(buf, thisms);
                    if (logfd) {
                        sprintf(xbuf, "<!--%05do( %s )-->\n", thisms, buf);
                        add2kml(xbuf);
                    }
                    strcat(buf, "\r\n");
                    doraw(buf);
                }
            }
        }
        // process serial GPS data
        gfd = gpst.gpsfd;
        greop = 1;
        n = 0;
        if (gpst.gpsfd < 0) {
            gfd = igpsfd;
            greop = 2;
        } else if (igpsfd >= 0 && FD_ISSET(igpsfd, &fds)) {
            for (;;) {
                n = read(igpsfd, buf, 511);
                if (n == -1 && errno == EAGAIN)
                    break;
                if (n <= 0) {
                    close(igpsfd);
                    gpst.lock = 0;
                    igpsfd = -1;
                    break;
                } else {
                    buf[n] = 0;
                    while (n && buf[n - 1] < ' ')       // strip trailing cr/lfs
                        buf[--n] = 0;
                    if (n) {
                        if (logfd) {
                            for (i = 0; i < n; i++)
                                if (buf[i] < ' ' || buf[i] > 0x7e)
                                    buf[i] = '.';
                            if (strlen(buf)) {
                                sprintf(xbuf, "<!--%05da( %s )-->\n", thisms, buf);
                                add2kml(xbuf);
                            }
                        }
                        strcat(buf, "\r\n");
                        doraw(buf);
                        if( mainlock ) // in case just keep open noise on main
                            mainlock--;
                        if (!mainlock)
                            getgpsinfo(buf);    // process gpst.alt GPS if main hasn't locked
                    }
                }
            }
            n = 0;
        }

        if (gfd >= 0 && FD_ISSET(gfd, &fds)) {
            for (;;) {
                n = read(gfd, buf, 511);
                if (n == -1 && errno == EAGAIN)
                    break;
                if (n <= 0) {
                    close(gfd);
                    gpst.lock = 0;
                    if (greop == 1)
                        gpst.gpsfd = -1;
                    else
                        igpsfd = -1;
                    gfd = -1;
                    fprintf(errfd, "gfd err %d\n", n);
                    fflush(errfd);
                    break;
                }
                if (thru != -1) {
                    if (n != write(thru, buf, n)) {
                        close(thru);
                        thru = -1;
                    }
                }
                if( mainlock )
                    mainlock--;

                buf[n] = 0;
                while (n && buf[n - 1] < ' ')   // strip trailing cr/lfs
                    buf[--n] = 0;
                if (!n)
                    continue;
                for (i = 0; i < n; i++)
                    if (buf[i] < ' ' || buf[i] > 0x7e)
                        buf[i] = '.';
                // print any undecoded below here
                if (logfd && buf[0] != 'J') {
                    sprintf(xbuf, "<!--%05dg( %s )-->\n", thisms, buf);
                    add2kml(xbuf);
                }
                strcat(buf, "\r\n");
                doraw(buf);
                if( buf[0] == 'J' ) {
                    calcobd(buf, thisms);
                    sprintf(xbuf, "<!--%05dg( %s )-->\n", thisms, buf);
                    add2kml(xbuf); //decoded version
                }
                else if( buf[0] == 'V' ) {
                    if( 1 == sscanf( &buf[1], "%08x", &ll ) ) {
                        more.v1flash = ll & ~llprev & ~llpvpv[0] & llpvpv[1] & llpvpv[2];
                        more.v1img = ll;
                        llpvpv[2] = llpvpv[1];
                        llpvpv[1] = llpvpv[0];
                        llpvpv[0] = llprev;
                        llprev = ll;
                    }
                }
                // no decode necessary
                else if( buf[0] == 'K' ) {
                    unsigned int ax,ay,az;
		    char *dot;
		    strcpy( xbuf, buf );
		    // zap decimal points
		    while( (dot = strstr(xbuf,".") ) )
		    	strcpy( dot, &dot[1] );
                    if( 3 == sscanf( &xbuf[2], "%d,%d,%d", &ax, &ay, &az ) ) {
                        more.xa = ax;
                        more.ya = ay;
                        more.za = az;
                    }
                }
                else {
                    i =  getgpsinfo(buf);
                    // fresh, good data plus lock, reset aux counter
                    if( i > 0 && gpst.lock )
                        mainlock = 100;
                }
            }
        }

        /* read commands from remote */
        for (i = 0; i < amax; i++) {
            if (acpt[i] == -1)
                continue;
            if (FD_ISSET(acpt[i], &fds)) {
                n = read(acpt[i], buf, 500);
                if (n <= 0) {
                    /* read error */
                    close(acpt[i]);
                    acpt[i] = -1;
                    continue;
                }
                buf[n] = 0;
                prtgpsinfo(i, buf);
            }
        }

        /* accept new connections */
        if (FD_ISSET(lstn, &fds)) {
            for (n = 0; n < amax; n++)
                if (acpt[n] == -1)
                    break;
            if (n < MAXCONN) {
                if (n >= amax)
                    amax++;
                u = sizeof(sin);
                raw[n] = 0;
                acpt[n] = accept(lstn, (struct sockaddr *) &sin, &u);
                i = fcntl(acpt[n], F_GETFL, 0);
                fcntl(acpt[n], F_SETFL, i | O_NONBLOCK);
                fprintf(errfd, "acpt %d %d\n", n, acpt[n]);
                fflush(errfd);
            }
        }

        /* accept new connections */
        if (FD_ISSET(lunix, &fds)) {
            for (n = 0; n < amax; n++)
                if (acpt[n] == -1)
                    break;
            if (n < MAXCONN) {
                if (n >= amax)
                    amax++;
                u = sizeof(sun);
                raw[n] = 0;
                acpt[n] = accept(lunix, (struct sockaddr *) &sun, &u);
                i = fcntl(acpt[n], F_GETFL, 0);
                fcntl(acpt[n], F_SETFL, i | O_NONBLOCK);
                fprintf(errfd, "acpt %d %d\n", n, acpt[n]);
                fflush(errfd);
            }
        }

        /* accept new connections */
        if (FD_ISSET(lthru, &fds)) {
            u = sizeof(sin);
            thru = accept(lthru, (struct sockaddr *) &sin, &u);
            i = fcntl(thru, F_GETFL, 0);
            fcntl(thru, F_SETFL, i | O_NONBLOCK);
        }

        /* accept new connections */
        if (FD_ISSET(lobdn, &fds)) {
            u = sizeof(sin);
            obdn = accept(lobdn, (struct sockaddr *) &sin, &u);
            i = fcntl(obdn, F_GETFL, 0);
            fcntl(obdn, F_SETFL, i | O_NONBLOCK);
            if (gpst.obdfd < 0)
                write(obdn, "NOT CONNECTED\n", 14);
        }

        /* accept new connections */
        if (FD_ISSET(lweb, &fds)) {
            char *c;
            u = sizeof(sin);
            n = accept(lweb, (struct sockaddr *) &sin, &u);
            i = read(n, xbuf, BUFLEN);
            /* for the GET / HTTP1.X, etc. */
            if (i < 0)
                break;
            xbuf[i] = 0;
            if (strstr(xbuf, "kml"))
                dokml(xbuf);
            else if (strstr(xbuf, "gpsdata.xml"))
                doxml();
            else if (strstr(xbuf, "dogmap.html"))
                dogmap();
            else if ((c = strstr(xbuf, "radar"))) {
                c += 5;
                i = atoi(c);
                if( !i )
                    i = 20;
                dorad(i);
            }
            else
                doweb();
            write(n, xbuf, strlen(xbuf));
            close(n);
        }

        if (thru != -1 && FD_ISSET(thru, &fds)) {
            n = read(thru, buf, 512);
            if (n <= 0) {
                close(thru);
                thru = -1;
            } else
                write(gpst.gpsfd, buf, n);
        }

        if (obdn > 0 && FD_ISSET(obdn, &fds)) {
            n = read(obdn, buf, 512);
            if (n <= 0) {
                close(obdn);
                obdn = -1;
            }
            // no else, not writing for now
        }

    }
    return 0;                   // quiet compiler
}

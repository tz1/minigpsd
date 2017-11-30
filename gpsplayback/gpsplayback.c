#include "minigpsd.h"
#include <errno.h>
#include <sys/termios.h>
#include <wait.h>

struct gpsstate gpst;
struct gpssats gpsat;
struct harley hstat = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

struct moredata {
    unsigned int v1img;
    unsigned int v1flash;
    unsigned int xa, ya, za;
};

struct moredata more = { 0, 0, 0, 0, 0 };

char *xbuf;

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
            sprintf(cbuf, ",P=%d.%06d %d.%06d", gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.llon / 1000000,
              abs(gpst.llon % 1000000));
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
            sprintf(cbuf, ",S=%d", gpst.lock);
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
              gpst.vdop / 1000, gpst.vdop % 1000 / 10, gpst.gtrk / 1000, gpst.gtrk % 1000, gpst.gspd / 1000, gpst.gspd % 1000,
              gpst.fix);
            break;
        case 'R':
            raw[conn] = !raw[conn];
            sprintf(cbuf, ",R=%d", raw[conn]);
            break;
        case 'E':
            sprintf(cbuf, ",E=%d.%02d %d.%02d %d.%02d", gpst.pdop / 1000, gpst.pdop % 1000 / 10, gpst.hdop / 1000,
              gpst.hdop % 1000 / 10, gpst.vdop / 1000, gpst.vdop % 1000 / 10);
            break;
        case 'H':
            sprintf(cbuf, ",H=%d.%03d", gpst.gtrk / 1000, gpst.gtrk % 1000);
            break;
        case 'J':
            strcat(xbuf, "\r\nSN EL AZM SG U");
            for (n = 0; n < gpst.nsats; n++) {
                sprintf(cbuf, "\r\n%d %02d %02d %03d %02d", gpst.sats[n].num < 0, abs(gpst.sats[n].num), gpst.sats[n].el,
                  gpst.sats[n].az, gpst.sats[n].sn);
                strcat(xbuf, cbuf);
            }
            cbuf[0] = 0;
            break;
        case 'Y':
            gettimeofday(&tv, NULL);
            sprintf(xbuf, "GPSD,Y=- %d.%06d %d:", (unsigned int) tv.tv_sec, (unsigned int) tv.tv_usec, gpst.nsats);
            for (n = 0; n < gpst.nsats; n++) {
                sprintf(cbuf, "%d %d %d %d %d:", abs(gpst.sats[n].num), gpst.sats[n].el, gpst.sats[n].az, gpst.sats[n].sn,
                  gpst.sats[n].num < 0);
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

static int stafd = -1;
static char *unixsock = "/tmp/.gpsd_ctrl_sock";
#include <gconf/gconf-client.h>
const char *mypath = "/apps/maemo/minigpsd/";

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

static void getagpslatlon()
{
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
    gpst.llat = (int) (d * 1000000.0);

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
    gpst.llon = (int) (d * 1000000.0);
}

static int gpsdport = 2947;
static int httpport = 8888;

#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>

static void getconfig()
{
    int tempport;
    char temppath[512];

    getagpslatlon();
    // atoi
    checkgconf("GPSDPORT", temppath);   // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        if (tempport > 0)
            gpsdport = tempport;
    }
    checkgconf("HTTPPORT", temppath);   // atoi
    if (strlen(temppath)) {
        tempport = atoi(temppath);
        if (tempport > 0)
            httpport = tempport;
    }
    checkgconf("UNIXSOCK", temppath);   // malloc
    if (strlen(temppath)) {
        unixsock = malloc(strlen(temppath) + 1);
        strcpy(unixsock, temppath);
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

int thisms = 0;
static int getms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    thisms = tv.tv_usec / 1000 + 1000 * (tv.tv_sec % 100);
    return thisms;
}

#include <sys/mman.h>
static char resplen[] = {
    4, 4, 8, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
    2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2,
    4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1,
    1, 2, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2,
    4, 4, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 0,
    0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

int main(int argc, char *argv[])
{
    int n, i, argp;
    int lstn = -1, lmax = 0, lunix = -1, lweb = -1;
    unsigned int u;
    fd_set fds, lfds;
    struct sockaddr_in sin;
    struct sockaddr_un sun;
    char buf[512], *c, *d;
    FILE *kmlinf;
    struct timeval tv;
    int pid, pipefrom[2];
    int fastfwd = 1;
    unsigned int ll, llprev = 0, llpvpv[3] = { 0, 0, 0 };

    if (!geteuid()) {
        fprintf(stderr, "Don't run as root!\n");
        exit(-2);
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: gpsreplay [-f <speed multiplier>] <kmz or kml files>\n");
        return -1;
    }

    memset(&gpst, 0, sizeof(gpst));
    gpst.gpsfd = 7;
    gpst.obdfd = 8;

    fprintf(stderr, "Startup\n");
    fflush(stderr);

    getconfig();

    for (n = 0; n < MAXCONN; n++)
        acpt[n] = -1;

    memset((char *) &sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    dobindlstn(&lstn, gpsdport);
    dobindlstn(&lweb, httpport);

    lunix = socket(AF_UNIX, SOCK_STREAM, 0);
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, unixsock);
    unlink(unixsock);
    if (bind(lunix, (struct sockaddr *) &sun, (int) sizeof(sun)))
        fprintf(stderr, "bind on unix sock failed\n");
    else if (listen(lunix, 3))
        fprintf(stderr, "listen on unix sock failed\n");

    xbuf = malloc(BUFLEN);

    listen(lstn, 3);
    listen(lweb, 3);
    lmax = lstn;

    if (lunix > lmax)
        lmax = lunix;
    if (lweb > lmax)
        lmax = lweb;

    FD_ZERO(&lfds);
    FD_SET(lstn, &lfds);
    FD_SET(lunix, &lfds);
    FD_SET(lweb, &lfds);

    getms();

    int vfd = open("/tmp/v1state", O_RDWR | O_CREAT, 0666);
    if (vfd < 0)
        return -1;
    memset(&u, 0, sizeof(u));
    write(vfd, &u, sizeof(u));
    unsigned int *v1img = mmap(0, sizeof(u), PROT_READ | PROT_WRITE, MAP_SHARED, vfd, 0);

    int mfd = open("/tmp/obd2state", O_RDWR | O_CREAT, 0666);
    if (mfd < 0)
        return -1;
    memset(buf, 0xa5, 512);
    write(mfd, buf, 512);
    unsigned int *memimg = mmap(0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);

    stafd = open("/tmp/mgpstate", O_RDWR | O_CREAT, 0644);
    write(stafd, &gpst, sizeof(gpst));
    write(stafd, &hstat, sizeof(hstat));
    write(stafd, &more, sizeof(more));

    for (argp = 1; argp < argc; argp++) {
        kmlinf = NULL;
        if (strstr(argv[argp], ".kml"))
            kmlinf = fopen(argv[argp], "r");
        else if (strstr(argv[argp], ".kmz")) {
            pipe(pipefrom);
            pid = fork();
            if (!pid) {
                dup2(pipefrom[1], STDOUT_FILENO);
                close(pipefrom[0]);
                close(pipefrom[1]);
                execlp("/usr/bin/unzip", "unzip", "-p", argv[argp], NULL);
                fprintf(stderr, "KMZ support requires unzip to be installed\n");
                exit(-1);
            }
            close(pipefrom[1]);
            kmlinf = fdopen(pipefrom[0], "r");
        } else if (strstr(argv[argp], "-f")) {
            fastfwd = atoi(argv[++argp]);
            continue;
        } else {
            fprintf(stderr, "Unknown arg %s is not kml/kmz file nor \"-f speed\"\n", argv[argp]);
            continue;
        }

        if (kmlinf == NULL)
            continue;

        for (;;) {
            waitpid(-1, &i, WNOHANG);
            if (feof(kmlinf))
                break;

            buf[0] = 0;
            fgets(buf, 512, kmlinf);

            if (strstr(buf, "TimeStamp")) {
                if (fastfwd > 1) {
                    i = fastfwd - 1;
                    while (i) {
                        buf[0] = 0;
                        fgets(buf, 512, kmlinf);
                        if (feof(kmlinf))
                            break;
                        if (strstr(buf, "TimeStamp"))
                            i--;
                    }
                    if (feof(kmlinf))
                        break;
                }
                getms();
                //                usleep( 1000 * (1001 - thisms % 1000) ); 
                continue;
            }

            if ((c = strstr(buf, "g( V"))) {
                c += 4;
                if (1 == sscanf(c, "%08x", &ll)) {
                    more.v1flash = ll & ~llprev & ~llpvpv[0] & llpvpv[1] & llpvpv[2];
                    more.v1img = ll;
                    llpvpv[2] = llpvpv[1];
                    llpvpv[1] = llpvpv[0];
                    llpvpv[0] = llprev;
                    llprev = ll;
                }
                continue;
            }
            if ((c = strstr(buf, "g( K,"))) {
                unsigned int ax, ay, az;
                char *dot;
                strcpy(xbuf, &buf[5]);
                // zap decimal points
                while ((dot = strstr(xbuf, ".")))
                    strcpy(dot, &dot[1]);
                if (3 == sscanf(xbuf, "%d,%d,%d", &ax, &ay, &az)) {
                    more.xa = ax;
                    more.ya = ay;
                    more.za = az;
                }
                continue;
            }

            if (!(c = strchr(buf, '$')))
                continue;

            d = strchr(c, '*');
            if (!d)
                continue;
            d += 3;
            *d++ = '\r';
            *d++ = '\n';
            *d = 0;
            doraw(c);
            if (c[1] == 'G')
                getgpsinfo(c);  // expects single null terminated strings (line ends dont matter)

            // add PLL code later
            //            usleep(100);

            if (c[1] == 'P') {
                c += 3;
                // thisms from PLL or buf tag
                if (strstr(c, "OBD")) {
                    if ((d = strstr(c, ",41,"))) {
                        unsigned int t[4], pid;
                        unsigned int ll;
                        int k;

                        d += 4;
                        sscanf(d, "%02x,%02x,%02x,%02x,%02x", &pid, &t[0], &t[1], &t[2], &t[3]);
                        ll = 0;
                        i = resplen[pid];
                        for (k = 0; k < i && k < 4; k++)
                            ll = (ll << 8) | t[k];
                        memimg[pid] = ll;
                    }
                } else {
                    if (strstr(c, "FUL") || strstr(c, "ODO")) {
                        c = strchr(c, ',');
                        if (!c)
                            continue;
                        c++;
                    }
                    c = strchr(c, ',');
                    if (!c)
                        continue;
                    c++;
                    c = strchr(c, ',');
                    if (!c)
                        continue;
                    c++;
                    d = strchr(c, '*');
                    if (d)
                        *d = 0;

                    calcobd(c, thisms);
                }
            }

            lseek(stafd, 0, SEEK_SET);
            write(stafd, &gpst, sizeof(gpst));
            write(stafd, &hstat, sizeof(hstat));
            write(stafd, &more, sizeof(more));

            fds = lfds;
            n = lmax;
#define MAXFD(fd) if (fd >= 0) { FD_SET(fd, &fds); if (fd > n) n = fd; }
            for (i = 0; i < amax; i++)
                MAXFD(acpt[i]);

            tv.tv_sec = 0;
            tv.tv_usec = 0;

            i = select(++n, &fds, NULL, NULL, &tv);
            if (i < 0)
                continue;       // if BT device detaches...
            // timeout added to reconnect
            if (i == 0)
                continue;

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
                    if (!i)
                        i = 20;
                    dorad(i);
                } else
                    doweb();
                write(n, xbuf, strlen(xbuf));
                close(n);
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
                    fprintf(stderr, "acpt %d %d\n", n, acpt[n]);
                    fflush(stderr);
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
                    fprintf(stderr, "acpt %d %d\n", n, acpt[n]);
                    fflush(stderr);
                }
            }

        }
        fclose(kmlinf);
    }
    return 0;                   // quiet compiler
}

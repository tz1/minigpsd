#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/termios.h>

static int schedule[16] = {12,13,5,12,13,0,-1};

static char resplen[] = {
    4, 4, 8, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
    2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2,
    4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1,
    1, 2, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2,
    4, 4, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 0,
    0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char rate[] = {
    0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 2,
    5, 5, 2, 1, 5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 2, 2,
    0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    0, 0, 2, 2, 2, 5, 2, 5, 5, 5, 5, 5, 5, 2, 2, 0,
    0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static int sfd = -1;
static unsigned int *memimg;
static char buf[512];

static void writestr(char *c)
{
    int n;
    if( sfd < 0 )
        return;
    //    printf( "W:%s\n", c );
    n = write(sfd, c, strlen(c));
    if( n == strlen(c) )
        return;
    close( sfd );
    sfd = -1;
}


static int tmotune = 500000; // wait X uS for response, but tune

static int readstr(int tmo)
{
    int n, m;
    fd_set fds;
    struct timeval tv;

    buf[0] = 0;
    m = 0;

    if( sfd < 0 )
        return -1;

    do {
        FD_ZERO(&fds);
        FD_SET(sfd, &fds);
        n = sfd + 1;
        tv.tv_sec = tmo/1000000;
        tv.tv_usec = tmo % 1000000;
        n = select(n, &fds, NULL, NULL, &tv);
        if( !n )
            return 1; // timeout
        //        usleep(50000); // let the buffer fill
        n = read(sfd, &buf[m], 512 - m);
        if (n < 0) {
            close( sfd );
            sfd = -1;
            return -1;
        }
        m += n;
        buf[m] = 0;
        buf[m+1] = 0;
	if( !n ) {
            printf( "TMO:[%s]\n", buf );
            return 1;
        }
    } while (!strchr(buf, '>'));
    //    m = tmo - tv.tv_usec; // leftover
    //    tmotune = m * 2;
    //    printf( "R:%s\n", buf );
    return 0;
}

// from minigpsd
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

static int obdpid = -1;
static char obdrfc[20] = "1";
static char obdbtaddr[32] = "";
static void reconn(int signo)
{
    int status = 0, child = -1;

    child = waitpid(-1, &status, WNOHANG);
    if( child == obdpid ){
        obdpid = -1;
        if (!(obdpid = fork())) {
            execlp("sh", "sh", "/usr/bin/btconnect.sh", obdrfc, obdbtaddr, NULL);
            exit(-1);
        }
    }
    signal(SIGCHLD, reconn);
}

void fixserio(int fd)
{
    struct termios termst;
    if (fd < 0)
        return;
    if (-1 == tcgetattr(fd, &termst))
        return;
    cfmakeraw(&termst);

    //    termst.c_iflag |= IGNCR;
    //    termst.c_lflag |= ICANON;

    tcsetattr(fd, TCSANOW, &termst);
    tcflush(fd, TCIOFLUSH);
}

static char *pidlockfile = "/tmp/miniobd2d.pid";

static void setupobd() {
    char tempbuf[80] = "", obdpath[80] = "";
    int i;

    checkgconf("OBDdev", obdpath);
    if (strlen(obdpath)) {
        if (obdpid < 0 && !strncmp(obdpath, "/dev/rfcomm", 11)) {
            strcpy(obdrfc, &obdpath[11]);
            checkgconf("OBDaddr", tempbuf);
            if (strlen(tempbuf)) {
                strcpy(obdbtaddr, tempbuf);
                if (!(obdpid = fork())) {
                    execlp("sh", "sh", "/usr/bin/btconnect.sh", obdrfc, obdbtaddr, NULL);
                    exit(-1);
                }
                signal(SIGCHLD, reconn);
                sleep(3); // ping, handshake
                i = 50;
                while( i-- ) {
                    if (!access(obdpath, F_OK))
                        break;
                    else
                        usleep(250000);
                }
            }
        }
        if (!access(obdpath, F_OK) && (sfd = open(obdpath, O_RDWR)) )
            fixserio(sfd);
    }
}

int mfd = -1;

static void teardown(int signo)
{
    signal(SIGCHLD, SIG_IGN);

    if (obdpid > 0) {
        kill(obdpid, SIGINT);
        sleep(2);
        kill(obdpid, SIGKILL);
    }
    unlink(pidlockfile);
    memset( memimg, 0xa5, 512 );
    exit(0);
}

static int startup()
{
    char *c;
    int restart = 0, i;

    for (;;) {

        while( sfd < 0 ) {
            printf( "COMM RESTART\n" );
            restart = 1;
            setupobd();
            if( sfd < 0 )
                sleep(5); // time to connect
        }

        writestr("\r");
        usleep(300000);
        writestr("ATSP0\r");
        // we will eventually get here from other timeouts, but we
        // need a timeout to close the port if the BT socket connection
        // is broken and read(sfd...) keeps returning zero chars.
        if( readstr(500000) ) {
            if( sfd >= 0 )
                close(sfd);
            sfd = -1;
            continue;
        }
        writestr("ATE0\r");
        if( readstr(300000) ) continue;
        writestr("ATH1\r");
        if( readstr(300000) ) continue;
        writestr("01 00\r");
        do {
            i = readstr(5000000);
        } while( i > 0 );
        if( i < 0 ) continue;
        c = strstr(buf, "UNAB");
        if (c)
            continue;
        c = strstr(buf, "41 00 ");
        if (c)
            break;
        printf( "NOSYNC:%s\n", buf );
    }
    printf( "RECONNECTED\n" );
    return restart;
}

static int mgdfd = -1;
static void startsock()
{
    struct sockaddr_in sin;
    unsigned int ll;

    printf( "log try reconnect\n" );
    mgdfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (mgdfd < 0)
        return;
    memset((char *) &sin, 0, sizeof(sin));
    ll = inet_addr("127.0.0.1");
    memcpy(&sin.sin_addr, &ll, sizeof(ll));
    sin.sin_family = AF_INET;

    char c[50];
    ll = 32947;
    checkgconf("GPSANNO", c);
    if (strlen(c))
        ll = atoi(c);
    if (ll < 1024 || ll > 65535 )
        ll = 32947;

    sin.sin_port = htons(ll);
    if (0 != connect(mgdfd, (struct sockaddr *) &sin, sizeof(sin))) {
        close(mgdfd);
        mgdfd = -1;
    }
    printf( "LOG RECONNECT\n" );
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

static void addnmeacksum(char *c)
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

int main()
{
    unsigned int ll, map[] = { 0, 0, 0 };
    unsigned int t[8];
    char match[20];
    int i, j, k, cnt, pidclk, pid;
    char *c, *d;
    int resched[16];


    if (!geteuid()) {
        fprintf(stderr, "Don't run as root!\n");
        exit(-2);
    }

    if (pilock())
        exit(-1);

    mfd = open("/tmp/obd2state", O_RDWR | O_CREAT, 0666);
    if (mfd < 0)
        return -1;
    memset(buf, 0xa5, 512);
    write(mfd, buf, 512);
    memimg = mmap(0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);

    signal(SIGTERM, teardown);
    signal(SIGINT, teardown);
    signal(SIGQUIT, teardown);

    startup();
    startsock();

    cnt = 0;
    for (;;) {
        if( !cnt )
            tmotune = 1000000;
            for (j = 0; j < 3; j++) {
                sprintf(match, "01 %02X\r", j * 0x20);
                writestr(match);
                sprintf(match, "41 %02X ", j * 0x20);
                readstr(tmotune);
                c = strstr(buf, match);
                if (!c) {
                    printf( "RESPERR:%s\n", buf );
                    startup();
                    j = 0;
                    continue;
                }

                map[j] = 0;
                do {
                    c += 6;

                    i = sscanf(c, "%02x %02x %02x %02x", &t[0], &t[1], &t[2], &t[3]);
                    if (i < 4)
                        return -1;
                    map[j] |= t[0] << 24;
                    map[j] |= t[1] << 16;
                    map[j] |= t[2] << 8;
                    map[j] |= t[3];
                    
                    c = strstr(&c[11], match);
                } while(c);

                if (!(map[j] & 1))
                    break;
                else
                    map[j] ^= 1;

                cnt = 0;
            }
        cnt++;

        map[0] &= 0x3fffffff;

        // fixme - reread schedule?  gconf event?
        memcpy( resched, schedule, sizeof(schedule) );
        pidclk = 0;
        for( j = 0 ; j < 12 && resched[j] > 0; j++ ) {
            i = resched[j] - 1;
            if (!(map[i >> 5] & (0x80000000 >> (i & 0x1f))))
                resched[j] = 0; // unsupported PID, replace with zero
        }

        for (j = 1; j < 32 * 3; j++) {
            i = j - 1;
            if (!(map[i >> 5] & (0x80000000 >> (i & 0x1f))))
                continue;
            if (cnt > 1 && rate[j] < 2)
                continue;
            pid = j;

            if( pidclk > 12 || resched[pidclk] < 0 ) // end of list
                pidclk = 0;
            if( resched[pidclk] > 0 ) { // priority PID
                pid = resched[pidclk];
                j--; //backup (pre-cancel out forloop increment)
            }
            pidclk++;

            k = 3;
            while( k > 0 ) {
                sprintf(match, "01 %02X\r", pid);
                writestr(match);
                sprintf(match, "41 %02X ", pid);
                buf[0] = 0;
                readstr(tmotune);
                c = strstr(buf, match);
                if (c)
                    break;
                printf( "resperr%d:%s\n", k, buf );
                k--;
                if( !k && startup() ) {
                    cnt = 0; // if reconnect, do full restart
                    j = 32767;
                }
            }
            if( !k )
                continue;
            // clean and parse
            c = buf;

            match[2] = ',';
            match[5] = ',';
            while( *c ) {
                d = c;
                while( *d ) {
                    if( *d == ' ' )
                        *d = ',';
                    else if( *d < ' ' ) {
                        *d++ = 0;
                        break;
                    }
                    d++;
                }

                if( !strstr(c, match) ) {
                    c = d;
                    continue;
                }

                if( c[strlen(c)-1] == ',' )
                    c[strlen(c)-1] = 0;

                if (mgdfd >= 0) {
                    char tbuf[256];
                    strcpy( tbuf, "$PDOBD," );
                    strcat( tbuf, c );
                    addnmeacksum(tbuf);

                    k = write(mgdfd, tbuf, strlen(tbuf));
                    if (k != strlen(tbuf) ) {
                        printf( "LOG DISCONNECT\n" );
                        close(mgdfd);
                        mgdfd = -1;
                    }
                }

                c = 6 + strstr(c, match);
                i = sscanf(c, "%02x,%02x,%02x,%02x", &t[0], &t[1], &t[2], &t[3]);
                ll = 0;
                i = resplen[pid];
                for (k = 0; k < i && k < 4; k++)
                    ll = (ll << 8) | t[k];
                memimg[pid] = ll;
                //printf( "[%02X %X]\n", pid, ll );
                c = d;
            }            
            
        }
        if( mgdfd < 0 )
            startsock();
    }
}

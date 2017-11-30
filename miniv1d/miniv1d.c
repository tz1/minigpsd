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


static int sfd = -1;
static unsigned int *memimg;

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

static int v1dpid = -1;
static char v1drfc[20] = "1";
static char v1dbtaddr[32] = "";
static void reconn(int signo)
{
    int status = 0, child = -1;

    child = waitpid(-1, &status, WNOHANG);
    if( child == v1dpid ){
        v1dpid = -1;
        if (!(v1dpid = fork())) {
            execlp("sh", "sh", "/usr/bin/btconnect.sh", v1drfc, v1dbtaddr, NULL);
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

static char *pidlockfile = "/tmp/miniv1d.pid";

static void setupv1d() {
    char tempbuf[80] = "", v1dpath[80] = "";
    int i;

    checkgconf("VV1dev", v1dpath);
    if (strlen(v1dpath)) {
        if (v1dpid < 0 && !strncmp(v1dpath, "/dev/rfcomm", 11)) {
            strcpy(v1drfc, &v1dpath[11]);
            checkgconf("VV1addr", tempbuf);
            if (strlen(tempbuf)) {
                strcpy(v1dbtaddr, tempbuf);
                if (!(v1dpid = fork())) {
                    execlp("sh", "sh", "/usr/bin/btconnect.sh", v1drfc, v1dbtaddr, NULL);
                    exit(-1);
                }
                signal(SIGCHLD, reconn);
                sleep(3); // ping, handshake
                i = 50;
                while( i-- ) {
                    if (!access(v1dpath, F_OK))
                        break;
                    else
                        usleep(250000);
                }
            }
        }
        if (!access(v1dpath, F_OK) && (sfd = open(v1dpath, O_RDWR)) )
            fixserio(sfd);
    }
}

int vfd = -1;

static void teardown(int signo)
{
    signal(SIGCHLD, SIG_IGN);

    if (v1dpid > 0) {
        kill(v1dpid, SIGINT);
        sleep(2);
        kill(v1dpid, SIGKILL);
    }
    unlink(pidlockfile);
    memset( memimg, 0, 4 );
    exit(0);
}

static int mgdfd = -1;
static void startsock()
{
    struct sockaddr_in sin;
    unsigned int ll;

    mgdfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (mgdfd < 0)
        return;
    memset((char *) &sin, 0, sizeof(sin));
    ll = inet_addr("127.0.0.1");
    memcpy(&sin.sin_addr, &ll, sizeof(ll));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(32947);
    if (0 != connect(mgdfd, (struct sockaddr *) &sin, sizeof(sin))) {
        close(mgdfd);
        mgdfd = -1;
    }
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

static int readstr()
{
    int n, m;
    int tmo = 100000;
    fd_set fds;
    struct timeval tv;
    unsigned char buf[512];
    int state = 0;
    unsigned int ll = 0, llprev = 0, llpvpv[4] = {0,0,0,0};
    int synccnt = 0;

    if( sfd < 0 )
        return 1;

    do {
        n = read(sfd, buf, 512);
    } while ( n == 512 );

    for (;;) {
        FD_ZERO(&fds);
        FD_SET(sfd, &fds);
        n = sfd + 1;
        tv.tv_sec = tmo/1000000;
        tv.tv_usec = tmo % 1000000;
        n = select(n, &fds, NULL, NULL, &tv);
        if( !n ) {
            sleep(1); // no data, wait to restart
            return 1; // timeout
        }
        usleep(25000); // let the buffer fill
        n = read(sfd, buf, 512);
        if (n <= 0) {
            close( sfd );
            sfd = -1;
            return 1;
        }
        m = 0;
        while( m < n ) {
            switch( state ) {
            case 0: // looking for sync - 32 consecutive zeros
                for( ; m < n ; m++ ) {
                    if( buf[m] < 0xf4 )
                        synccnt++;
                    else
                        synccnt = 0;
                    if( synccnt > 32 ) {
                        synccnt = 0;
                        state = 1;
                        m++;
                        break;
                    }
                }
            case 1: // looking for start
                for( ; m < n ; m++ ) 
                    if( buf[m] > 0xf4 ) {
                        ll = 0;
                        state = 2;
                        synccnt = 0;
                        m++;
                        break;
                    }
                if( m >= n )
                    break;
            case 2:
                for( ; m < n && synccnt < 32; m++ ) {
                    synccnt++;
                    ll <<= 1;
                    // 00 80 c0 e0 | f0 x f8 | fc fe ff 
                    if( buf[m] > 0xf4 )
                        ll |= 1;
                }
                if( synccnt >= 32 ) {
                    state = 0;
                    //if( ll != llprev )
                    { //Always report
                        memimg[1] = ll & ~llprev & ~llpvpv[0] & llpvpv[1] & llpvpv[2]; // flashing
                        //llpvpv[3] = llpvpv[2];
                        llpvpv[2] = llpvpv[1];
                        llpvpv[1] = llpvpv[0];
                        llpvpv[0] = llprev;
                        llprev = ll;
                        *memimg = ll;
#if 0
                        int i;
                        for( i = 0 ; i < 32 ; i++ )
                            printf( "%d", (ll >> (31 - i )) & 1 );
                        printf(  "\n" );
#else
                        // decode ll into countchar, sigstr, bands, dirs, mute and print friendly
#endif
                        if (mgdfd < 0)
                            // maybe limit to every few seconds?
                            startsock();
                        if (mgdfd >= 0) {
                            char c[40];
                            int k;
                            sprintf( c , "$PRDV1,%08X,%08x", ll, memimg[1] );
                            addnmeacksum(c);
                            k = write(mgdfd, c, strlen(c));
                            if (k != strlen(c) ) {
                                close(mgdfd);
                                mgdfd = -1;
                            }
                        }
                    }
                    synccnt = 0;
                }
                break;
            }
        }
    }
    return 0;
}


static int startup()
{
    int restart = 0, i;

    for (;;) {

        while( sfd < 0 ) {
            printf( "COMM RESTART\n" );
            restart = 1;
            setupv1d();
            if( sfd < 0 )
                sleep(5); // time to connect
        }
        printf( "RESYNC\n" );
        i = readstr();
        if (!i)
            break;
        printf( "FAILED\n" );
    }
    printf( "RECONNECTED\n" );
    return restart;
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

int main()
{
    unsigned int ll[2] = {0,0};

    if (!geteuid()) {
        fprintf(stderr, "Don't run as root!\n");
        exit(-2);
    }

    if (pilock())
        exit(-1);

    vfd = open("/tmp/v1state", O_RDWR | O_CREAT, 0666);
    if (vfd < 0)
        return -1;
    write(vfd, ll, sizeof(ll));
    memimg = mmap(0, sizeof(ll), PROT_READ | PROT_WRITE, MAP_SHARED, vfd, 0);

    signal(SIGTERM, teardown);
    signal(SIGINT, teardown);
    signal(SIGQUIT, teardown);

    startsock();
    startup();

    for (;;) {
        if( readstr() )
            startup();
    }
}

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
static unsigned char buf[512];
static int buflen = 0;

static void writestr(char *c)
{
    int n;
    if (sfd < 0)
        return;
    n = write(sfd, c, strlen(c));
    if (n == strlen(c))
        return;
    close(sfd);
    sfd = -1;
}

static int readstr()
{
    int n, m;
    int tmo = 1000000;
    fd_set fds;
    struct timeval tv;

    buf[0] = 0;
    m = 0;
    buflen = 0;
    if (sfd < 0)
        return -1;

    for (;;) {
        FD_ZERO(&fds);
        FD_SET(sfd, &fds);
        n = sfd + 1;
        tv.tv_sec = tmo / 1000000;
        tv.tv_usec = tmo % 1000000;
        n = select(n, &fds, NULL, NULL, &tv);
        if (!n)
            return 1;           // timeout
        //        usleep(50000); // let the buffer fill
        n = read(sfd, &buf[buflen], 28 - buflen);
        if (n < 0) {
            close(sfd);
            sfd = -1;
            return -1;
        }
        buflen += n;
        if (!n)
            return 1;
        // short circuit checks if synced
        if (buflen == 28 && buf[0] == 'A' && buf[27] == 'Z')
            return 0;

        for (m = 0; m < buflen; m++)
            if (buf[m] == 'A')
                break;
        if (m >= buflen)
            continue;
        if (m) {
            memcpy(buf, &buf[m], buflen - m);
            buflen -= m;
        }
        if (buflen < 28)
            continue;

        if (buf[27] == 'Z')
            return 0;
        buf[0] = 0;
    }
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

static int sdofpid = -1;
static char sdofrfc[20] = "1";
static char sdofbtaddr[32] = "";
static void reconn(int signo)
{
    int status = 0, child = -1, i;

    child = waitpid(-1, &status, WNOHANG);
    if (child == sdofpid) {
        sdofpid = -1;
        if (!(sdofpid = fork())) {
            for( i = 3; i < 1024; i++ )
                close(i);
            execlp("sh", "sh", "/usr/bin/btconnect.sh", sdofrfc, sdofbtaddr, NULL);
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

static char *pidlockfile = "/tmp/minisdofd.pid";

static void setupsdof()
{
    char tempbuf[80] = "", sdofpath[80] = "";
    int i;

    checkgconf("SDOFdev", sdofpath);
    if (strlen(sdofpath)) {
        if (sdofpid < 0 && !strncmp(sdofpath, "/dev/rfcomm", 11)) {
            strcpy(sdofrfc, &sdofpath[11]);
            checkgconf("SDOFaddr", tempbuf);
            if (strlen(tempbuf)) {
                strcpy(sdofbtaddr, tempbuf);
                if (!(sdofpid = fork())) {
                    execlp("sh", "sh", "/usr/bin/btconnect.sh", sdofrfc, sdofbtaddr, NULL);
                    exit(-1);
                }
                signal(SIGCHLD, reconn);
                sleep(3);       // ping, handshake
                i = 50;
                while (i--) {
                    if (!access(sdofpath, F_OK))
                        break;
                    else
                        usleep(250000);
                }
            }
        }
        if (!access(sdofpath, F_OK) && (sfd = open(sdofpath, O_RDWR)))
            fixserio(sfd);
    }
}

int mfd = -1;

static void teardown(int signo)
{
    signal(SIGCHLD, SIG_IGN);
    writestr(" ");
    readstr();
    if (sdofpid > 0) {
        kill(sdofpid, SIGINT);
        sleep(2);
        kill(sdofpid, SIGKILL);
    }
    unlink(pidlockfile);
    memset(memimg, 0xa5, 512);
    exit(0);
}

static int startup()
{
    int restart = 0, i;

    writestr(" ");
    for (;;) {
        while (sfd < 0) {
            printf("COMM RESTART\n");
            restart = 1;
            setupsdof();
            if (sfd < 0)
                sleep(5);       // time to connect
            writestr("\007");
        }

        i = readstr();
        if (i < 0)
            continue;
        if (i == 0)
            break;
    }
    printf("RECONNECTED\n");
    return restart;
}

static int mgdfd = -1;
static void startsock()
{
    struct sockaddr_in sin;
    unsigned int ll;

    printf("log try reconnect\n");
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
    if (ll < 1024 || ll > 65535)
        ll = 32947;

    sin.sin_port = htons(ll);
    if (0 != connect(mgdfd, (struct sockaddr *) &sin, sizeof(sin))) {
        close(mgdfd);
        mgdfd = -1;
    }
    printf("LOG RECONNECT\n");
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
        if (pid <= 0 || kill(pid, 0) == -1)     //stale?
            unlink(pidlockfile);
        else
            return 1;           // active
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
    int i, k, cnt;

    if (!geteuid()) {
        fprintf(stderr, "Don't run as root!\n");
        exit(-2);
    }

    if (pilock())
        exit(-1);

    mfd = open("/tmp/sdofstate", O_RDWR | O_CREAT, 0666);
    if (mfd < 0)
        return -1;
    memset(buf, 0xa5, 52);
    write(mfd, buf, 52);
    memimg = mmap(0, 52, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);

    signal(SIGTERM, teardown);
    signal(SIGINT, teardown);
    signal(SIGQUIT, teardown);

    startup();
    startsock();

    cnt = 0;
    for (;;) {
        char outstr[256];

        for (i = 0; i < 13; i++)
            memimg[i] = (buf[i * 2 + 1] << 8) | buf[i * 2 + 2];

        sprintf(outstr, "$PD6DF,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
          memimg[0], memimg[1], memimg[2], memimg[3], memimg[4], memimg[5], memimg[6], memimg[7], memimg[8], memimg[9], memimg[10],
          memimg[11], memimg[12]);
        addnmeacksum(outstr);

        //        printf( "%s\n", outstr );        fflush(stdout);

        i = readstr();
        if (i != 0)
            startup();

        if (mgdfd >= 0) {
            k = write(mgdfd, outstr, strlen(outstr));
            if (k != strlen(outstr)) {
                printf("LOG DISCONNECT\n");
                close(mgdfd);
                mgdfd = -1;
            }
        } else;                 //startsock();
    }
}

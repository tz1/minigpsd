#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
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
x() {
    checkgconf("USEGPS", temppath);
    if ((atoi(temppath) == 1)) {
        checkgconf("GPSdev", temppath);
        if (strlen(temppath)) {
            if (!strncmp(temppath, "/dev/rfcomm", 11)) {
                gpspath = malloc(strlen(temppath) + 1);
                strcpy(gpspath, temppath);
                strcpy(gpsrfc, &temppath[11]);
                checkgconf("GPSaddr", temppath);

                strcpy(gpsrfc, &temppath[11]);
                checkgconf("GPSaddr", temppath);
                if (strlen(temppath)) {
                    strcpy(gpsbtaddr, temppath);
                    if (!(gpspid = fork())) {
                        system("rfcomm connect 5 %s");
                        exit(-1);
                    }
                }
            }
        }
    }
}


static char string[64];
static void readtonull(int fd)
{
    int l;
    char *b = string;
    for (;;) {
        l = read(fd, b, 1);
        if (l <= 0)
            continue;
        if (!*b++)
            break;
    }
    printf("%s\n", string);
}

static unsigned char ephbuf[256 * 1024];
static const unsigned char setagps[8] = { 0xa0, 0xa1, 0x00, 0x01, 0x35, 0x35, 0x0d, 0x0a };
static const unsigned char agpsresp[9] = { 0xa0, 0xa1, 0x00, 0x02, 0x83, 0x35, 0xb6, 0x0d, 0x0a };
static const unsigned char agpsena[9] = { 0xa0, 0xa1, 0x00, 0x02, 0x33, 0x01, 0x32, 0x0d, 0x0a };

int main(int argc, char *argv[])
{
    int fd;
    //    char gpsdev[64] = "/dev/rfcomm0";
    char gpsdev[64] = "/dev/ttyUSB0";
    unsigned i;

    if( argc > 1 )
        strcpy( gpsdev, argv[1] );

    fd = open(gpsdev, O_RDWR);
    if (fd < 0)
        return -10;

    struct termios tio;
    if ((tcgetattr(fd, &tio)) == -1)
        return -1;
    cfmakeraw(&tio);
    if ((tcsetattr(fd, TCSAFLUSH, &tio)) == -1)
        return -1;
    // add: find baud rate

    unsigned char *ephdata;
    long ephbytes;

    ephdata = ephbuf;
    int ofd = open("Eph.dat", O_RDONLY);
    if (ofd < 0)
        return -2;
    ephbytes = read(ofd, ephbuf, 256 * 1024);
    if (ephbytes < 65536)
        return -3;

    // checksum
    unsigned char csuma, csumb = 0;
    for (i = 0; i < 0x10000; i++)
        csumb += ephdata[i];
    csuma = csumb;
    for (; i < ephbytes; i++)
        csuma += ephdata[i];

    // AGPS download startup: send command, get ack - maybe put in loop?
    printf( "Startup\n" );
    do {
        do { // flush input buffer
            i = read(fd, string, 64);
        } while( i == 64 );
        write(fd, setagps, 8);
        i = 0;
        while ( i < 0 || string[0] != '\xa0')
            i = read(fd, string, 1);
        while (i < 64) {
            i += read(fd, &string[i], 64 - i);
            if (i > 0 && string[i-1] == 0x0a)
                break;
        }
    } while( memcmp(&string[i - 9], agpsresp, 9) );
    printf( "Venus Ready\n" );
    
    /* start the transmission */
    sprintf(string, "BINSIZE = %ld Checksum = %d Checksumb = %d ", ephbytes, csuma, csumb);
    printf("%s:", string);
    write(fd, string, strlen(string) + 1);
    readtonull(fd);

#define BLKSIZ 8192
    while (ephbytes > 0) {
        printf("%ld left:", ephbytes);
        write(fd, ephdata, ephbytes > BLKSIZ ? BLKSIZ : ephbytes);
        readtonull(fd);        // OK
        ephbytes -= BLKSIZ;
        ephdata += BLKSIZ;
    }
    // Status "END" or "Error2"
    readtonull(fd);            // END

    sleep(1);
    write(fd, agpsena, 9);
    // maybe get ack?
    sleep(1);
    close(fd);
    return 0;
}

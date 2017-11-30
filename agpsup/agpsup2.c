#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

static char string[1024]; // ephem, 87 bytes is longest known PL but...
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
int serfd = -1;
static void sendpayload(unsigned len, char *payload)
{
    unsigned char abuf[2] = {0xa0, 0xa1};
    unsigned char zbuf[3] = "*/r/n";
    write( fd, abuf, 2 );
    abuf[0] = len >> 8;
    abuf[1] = len;
    write( fd, abuf, 2 );
    write( fd, payload, len );

    unsigned char c = 0;
    while( len-- )
        c ^= *payload++;
    zbuf[0] = c;
    write( fd, zbuf, 3 );
}

static int getresponse()
{
    unsigned char c = 0;
    int i, j, l=1000;
    i = 0;
    
    while ( i < 0 || string[0] != '\xa0')
        i = read(serfd, string, 1);
    while (i < 3) {
        j = read(serfd, &string[i], 3 - i);
        if( j <= 0 )
            continue;
        i += j;
    }
    l = 0xff00 & (string[2] << 8);
    l |= 0xff & string[3];
    if( l > 1000 )
        l = 1000;
    while (i < 3+l) {
        j = read(serfd, &string[i], 3+l - i);
        if( j <= 0 )
            continue;
        i += j;
    }
    j = l;
    while( j-- )
        c ^= string[3+j];
    while (i < 6+l) {
        j = read(serfd, &string[i], 6+l - i);
        if( j <= 0 )
            continue;
        i += j;
    }
    if( c != string[4+l] )
        return -1;
    if( string[5+l] != `\r` )
        return 1;
    if( string[6+l] != `\n` )
        return 1;
    if( string[1] != `\xa1` )
        return 1;
    return 0;
}

static unsigned char ephbuf[256 * 1024];

static const unsigned char setagps[8] = { 0xa0, 0xa1, 0x00, 0x01, 0x35, 0x35, 0x0d, 0x0a };
static const unsigned char agpsresp[9] = { 0xa0, 0xa1, 0x00, 0x02, 0x83, 0x35, 0xb6, 0x0d, 0x0a };
static const unsigned char agpsena[9] = { 0xa0, 0xa1, 0x00, 0x02, 0x33, 0x01, 0x32, 0x0d, 0x0a };

int main(int argc, char *argv[])
{
    char *gpsdev = "/dev/ttyUSB0";
    unsigned i;

    serfd = open(gpsdev, O_RDWR);
    if (serfd < 0)
        return -10;

    struct termios tio;
    if ((tcgetattr(serfd, &tio)) == -1)
        return -1;
    cfmakeraw(&tio);
    if ((tcsetattr(serfd, TCSAFLUSH, &tio)) == -1)
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
    do { // flush input buffer
        i = read(serfd, string, 64);
    } while( i == 64 );
    printf( "Startup\n" );
    sendpayload( "\x35", 1 );
    //    write(serfd, setagps, 8);
    getresponse();
    /*
    i = 0;
    while ( i < 0 || string[0] != '\xa0')
        i = read(serfd, string, 1);
    while (i < 64) {
        i += read(serfd, &string[i], 64 - i);
        if (i > 0 && string[i-1] == 0x0a)
            break;
    }
    */
    printf( "Venus Ready\n" );
    if (memcmp(&string[i - 9], agpsresp, 9))
        return -6;
    
    /* start the transmission */
    sprintf(string, "BINSIZE = %ld Checksum = %d Checksumb = %d ", ephbytes, csuma, csumb);
    printf("%s:", string);
    write(serfd, string, strlen(string) + 1);
    readtonull(serfd);

#define BLKSIZ 8192
    while (ephbytes > 0) {
        printf("%ld left:", ephbytes);
        write(serfd, ephdata, ephbytes > BLKSIZ ? BLKSIZ : ephbytes);
        readtonull(serfd);        // OK
        ephbytes -= BLKSIZ;
        ephdata += BLKSIZ;
    }
    // Status "END" or "Error2"
    readtonull(serfd);            // END

    sleep(1);
    sendpayload( "\x33\x01", 2 );

    //  write(serfd, agpsena, 9);
    // maybe get ack?
    sleep(1);
    close(serfd);
    return 0;
}

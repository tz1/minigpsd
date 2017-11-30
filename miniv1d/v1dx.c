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


static int sfd = 0;
static unsigned int *memimg;

static int readstr()
{
    int n, m;
    int tmo = 50000;
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
#if 1
                        int i;
                        for( i = 0 ; i < 32 ; i++ )
                            printf( "%d", (ll >> (31 - i )) & 1 );
                        printf(  "\n" );
#else
                        // decode ll into countchar, sigstr, bands, dirs, mute and print friendly
#endif
                    }
                    synccnt = 0;
                }
                break;
            }
        }
    }
    return 0;
}

int vfd = -1;
int main()
{
    unsigned int ll[2] = {0,0};


    vfd = open("/tmp/v1state", O_RDWR | O_CREAT, 0666);
    if (vfd < 0)
        return -1;
    write(vfd, ll, sizeof(ll));
    memimg = mmap(0, sizeof(ll), PROT_READ | PROT_WRITE, MAP_SHARED, vfd, 0);

        readstr();
}

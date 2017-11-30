#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/un.h>

int main(int argc, char *argv[])
{
    char datestr[512] = "",*c, *d, buf[512];
    FILE *kmlinf;
    int argp, pipefrom[2], pid;
    double la = 0.0,lo = 0.0;

    printf( "<?xml version=\"1.0\"?>\n<gpx version=\"1.0\" creator=\"kml2gpx\" xmlnx=\"http://www.topografix.com/GPX/1/0\">\n" );
    printf( "<trk><trkseg>\n" );

    for (argp = 1; argp < argc; argp++) {
        kmlinf = NULL;
        if( strstr(argv[argp], ".kml" ) )
            kmlinf = fopen(argv[argp], "r");
        else if( strstr(argv[argp], ".kmz" ) ) {
            pipe(pipefrom);
            pid = fork();
            if (!pid) {
                dup2(pipefrom[1], STDOUT_FILENO);
                close(pipefrom[0]);
                close(pipefrom[1]);
                execlp("/usr/bin/unzip", "unzip", "-p", argv[argp], NULL);
                exit(-1);
            }
            close(pipefrom[1]);
            kmlinf = fdopen( pipefrom[0], "r" );
        }

        if (kmlinf == NULL)
            continue;

        for (;;) {
            if (feof(kmlinf))
                break;

            fgets( buf, 512, kmlinf );

            if(( c = strstr( buf, "<TimeStamp><when>" ) )) {
                c += 17;
                strcpy( datestr, c );
                c = strstr( datestr, "<" );
                *c = 0;
                //                printf( "\n", datestr );


                continue;
            }

            if( !strchr( buf, '<' ) ) {
                double la1,lo1;

                if( !strlen(datestr) )
                    continue;

                sscanf( buf, "%lf,%lf", &lo1, &la1 );
                if( la1 == la && lo1 == lo )
                    continue;
                la = la1;
                lo = lo1;
                printf( "<trkpt lat=\"%.6f\" lon=\"%.6f\"><time>%s</time></trkpt>\n", la1, lo1, datestr );
            }


            if( !( c = strchr( buf, '$' ) ) )
                continue;

            d = strchr( c, '*' );
            if( !d )
                continue;
            d += 3;
            *d++ = '\r';
            *d++ = '\n';
            *d = 0;
            c++;
            if( !strncmp( c, "GPRMC", 5 ) ) {
            }
            if( !strncmp( c, "GPGLL", 5 ) ) {
            }
            if( !strncmp( c, "GPGGA", 5 ) ) {
            }


        }
        fclose(kmlinf);
    }
    printf( "</trkseg></trk></gpx>\n" );


    return 0;                   // quiet compiler
}

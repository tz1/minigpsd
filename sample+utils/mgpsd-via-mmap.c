#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include "minigpsdmmap.h"

int main()
{
    int file;
    struct gpsstate *data;

    file = open("/tmp/mgpstate", O_RDONLY);
    data = mmap(0, sizeof(struct gpsstate), PROT_READ, MAP_SHARED, file, 0);

    for (;;) {
        sleep(1);

        printf("lat=%f\n", (double) data->llat / 1000000.0);
        printf("lon=%f\n", (double) data->llon / 1000000.0);
        printf("alt=%f\n", (double) data->alt / 1000.0);

        printf("pdop=%f\n", (double) data->pdop / 1000.0);
        printf("hdop=%f\n", (double) data->hdop / 1000.0);
        printf("vdop=%f\n", (double) data->vdop / 1000.0);

        printf("spd=%f\n", (double) data->gspd / 1000.0);
        printf("trk=%f\n", (double) data->gtrk / 1000.0);

        printf("%d/%d/%02d\n", data->mo, data->dy, data->yr);
        printf("%02d:%02d:%02d.%03d\n", data->hr, data->mn, data->sc, data->scth);

        printf("lock=%d\n", data->lock);
        printf("fix=%d\n", data->fix);

        // BT status    printf( data->gpsfd, data->obdfd

        int sats = data->nsats;
        printf("sats=%d,", sats);
        printf(" used=%d\n", data->nused );
	printf("U Nu El Azm SN\n" );
        int i;
        for (i = 0; i < sats; i++)
            printf("%d %2d %2d %3d %2d\n", data->sats[i].num < 0, data->sats[i].num, data->sats[i].el, data->sats[i].az,
		   data->sats[i].sn);
    }
}

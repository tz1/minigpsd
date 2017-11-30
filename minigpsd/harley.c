#include "minigpsd.h"

static int crc(int *data, int len)
{
    unsigned char crc, dimg;
    unsigned char poly;
    int i;

    crc = 0xff;
    while (len--) {
        dimg = *data++;
        //      printf("%02x ", dimg);
        for (i = 0; i < 8; i++) {
            poly = 0;
            if (0x80 & (crc ^ dimg ))
                    poly = 0x1d;
            crc = (crc << 1) ^ poly;
            dimg <<= 1;
        }
    }
    //printf("= %02x\n", crc);
    return crc;
}

static unsigned short odolast = 0, fuellast = 0;

extern struct harley hstat;

void calcobd(char *outb, int mstime)
{
    int i, j, x;
    unsigned short y;
    int hex[8];
    char inb[512], *c, *d;

    c = outb;
    while( *c && (*c < '0' || *c > 'F') ) // remove leading J or whatever
        c++;
    d = inb;
    while( *c ) {
	if( (*c >= '0' || *c <= 'F') )
	       *d++ = *c;
	c++;
    }
    *d++ = 0;
#if 0
    // obdpros
    i = sscanf(inb, "%02x %02x %02x %02x %02x %02x %02x %02x",
               );
#endif
    // AVR
    i = sscanf(inb, "%02x%02x%02x%02x%02x%02x%02x%02x",
               &hex[0], &hex[1], &hex[2], &hex[3], &hex[4], &hex[5], &hex[6], &hex[7]);
    
    if (i < 5 || 0xc4 != crc(hex, i)) {
        sprintf(outb, "$PDERR,%d,", i);
        strcat(outb, inb); 
        return;
    }

    i--;
    x = hex[0] << 24;
    x |= hex[1] << 16;
    x |= hex[2] << 8;
    x |= hex[3];

    y = hex[4] << 8 | hex[5];

    if (x == 0x281b1002) {
        sprintf(outb, "$PDRPM,%d.%02d,", y / 4, y % 4 * 25);
        hstat.rpm = y * 250;
    } else if (x == 0x48291002) {
        sprintf(outb, "$PDSPD,%d.%03d,", y / 200, y % 200 * 5);
        hstat.vspd = y * 5;
    } else if (x == 0xa8491010) {
        sprintf(outb, "$PDHOT,%d,", hex[4]);
        hstat.engtemp = hex[4];
    } else if (x == 0xa83b1003) {
        y = hex[4];
        j = 0;
        if (y)
            while (y >>= 1)
                j++;
        else
            j = -1;
        sprintf(outb, "$PDGER,%d,", j);
        hstat.gear = j;
    } else if (x == 0x48da4039 && (hex[4] & 0xfc) == 0) {
        char turns[] = "NRLB";
        sprintf(outb, "$PDSGN,%c,", turns[hex[4]]);
        hstat.turnsig = hex[4];
    } else if ((x & 0xffffff7f) == 0xa8691006) {
        if (!(x & 0x80)) {
            hstat.odolastval = y;
            hstat.odolastval -= odolast;
	    if( hstat.odolastval < 0 )
	        hstat.odolastval += 65536;
            hstat.odoaccum += hstat.odolastval;
            if (mstime >= hstat.odolastms)
                hstat.odolastms = mstime - hstat.odolastms;
            else
                hstat.odolastms = 100000 + mstime - hstat.odolastms;
            sprintf(outb, "$PDODO,%d,%d,", hstat.odolastval, hstat.odolastms);
            odolast = y;
            hstat.odolastms = mstime;
        } else {
            odolast = 0;
            hstat.odolastms = mstime;
            hstat.odoaccum = 0;
            sprintf(outb, "$PDODO,-0,-0,");
        }
    } else if ((x & 0xffffff7f) == 0xa883100a) {
        if (!(x & 0x80)) {
            hstat.fuellastval = y;
            hstat.fuellastval -= fuellast;
	    if( hstat.fuellastval < 0 )
	        hstat.fuellastval += 65536;
            hstat.fuelaccum += hstat.fuellastval;
            if (mstime >= hstat.fuellastms)
                hstat.fuellastms = mstime - hstat.fuellastms;
            else
                hstat.fuellastms = 100000 + mstime - hstat.fuellastms;
            sprintf(outb, "$PDFUL,%d,%d,", hstat.fuellastval, hstat.fuellastms);
            fuellast = y;
            hstat.fuellastms = mstime;
        } else {
            fuellast = 0;
            hstat.fuellastms = mstime;
            hstat.fuelaccum = 0;
            sprintf(outb, "$PDFUL,-0,-0,");
        }
    } else if ((x & 0xffffffff) == 0xa8836112 && (hex[4] & 0xd0) == 0xd0) {
        sprintf(outb, "$PDGAS,%d,", hex[4] & 0x0f);
        hstat.full = hex[4] & 0x0f;
    } else if ((x & 0xffffff5d) == 0x483b4000) {
        sprintf(outb, "$PDCLU,");
        hstat.neutral = !!(hex[3] & 0x20);    // ! & 0x02
        hstat.clutch = !!(hex[3] & 0x80);
        switch (hex[3]) {
        case 0x02:
            sprintf(outb, "$PDCLU,xx,");
            break;
        case 0x82:
            sprintf(outb, "$PDCLU,xC,");
            break;
        case 0x20:
            sprintf(outb, "$PDCLU,Nx,");
            break;
        case 0xA0:
            sprintf(outb, "$PDCLU,NC,");
            break;
        }
    } else if (x == 0x68881003
      || x == 0x68FF1003 || x == 0x68FF4003 || x == 0x68FF6103 || x == 0xC888100E || x == 0xC8896103 || x == 0xE889610E) {
        sprintf(outb, "$PDPNG,");
    } else if ((x & 0xffffff7f) == 0x4892402a || (x & 0xffffff7f) == 0x6893612a) {
        sprintf(outb, "$PDOFF,");       // shutdown - lock
    } else if (x == 0x68881083) {
        sprintf(outb, "$PDMIL,");       // check engine
    } else {
        sprintf(outb, "$PDMSG,");
    }

    c = inb;
    while (*c) {
        if (*c == ' ')
            *c = ',';
        c++;
    }

    strcat(outb, inb);
    c = outb;
    while( *c )
        if( *c <  ' ' )
            *c = 0;
        else
            c++;
    addnmeacksum(outb);

}

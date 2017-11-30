#!/usr/bin/env python

import  struct
import mmap
import time

file = open("/tmp/mgpstate", "r+")

data = mmap.mmap(file.fileno(), 244)

print "lat,lon"
while True:
    gpstate = struct.unpack("21i80h",data)

    print "lat=",float(gpstate[0])/1000000
    print "lon=",float(gpstate[1])/1000000
    print "alt=",float(gpstate[2])/1000

    print "pdop=",float(gpstate[3])/1000
    print "hdop=",float(gpstate[4])/1000
    print "vdop=",float(gpstate[5])/1000

    print "spd=",float(gpstate[6])/1000
    print "trk=",float(gpstate[7])/1000

    print gpstate[10],"/", gpstate[9], "/",gpstate[8]
    print gpstate[11],":", gpstate[12], ":",gpstate[13], ".", str(gpstate[14]).zfill(3)

    print "lock=",gpstate[15]
    print "fix=", gpstate[16]

# BT status    print gpstate[17], gpstate[18]

    sats = gpstate[19]
    print "sats=", sats," used=", gpstate[20]

    for i in range(sats):
        j=21+i*4
        print gpstate[j],gpstate[j+1],gpstate[j+2],gpstate[j+3]

    time.sleep(0.3)
    
'''
struct satinfo {
    unsigned char num, el, az, sn;
};

struct gpsstate {
    int llat, llon, alt;
    int pdop, hdop, vdop;
    int gspd, gtrk;
    int yr, mo, dy, hr, mn, sc, scth;
    int lock, fix;
    int gpsfd, obdfd;
    int nsats, nused;
    struct satinfo sats[20];
};
'''

                                                

// data billboard
struct satinfo {
  // satellite number, elevation, azmuith, and signal
  // satellite number is NEGATIVE if used
    short num, el, az, sn;
};

struct gpsstate {
  // latitude, longitude in micro-degrees.  Altitude in feet * 1000
    int llat, llon, alt;
  // dilution of precision * 1000
    int pdop, hdop, vdop;
  // speed, mph * 1000, track, degrees * 1000
    int gspd, gtrk;
  // year (in century, 08 not 2008), month, day, hour, minute, second, thousanths
    int yr, mo, dy, hr, mn, sc, scth;
  // lock, 0-1.  fix from GPGSA
    int lock, fix;
  // -1 if device not connected
    int gpsfd, obdfd;
  // number of sats visible, number being used in fix
    int nsats, nused;
  // satellite table
    struct satinfo sats[20];
};

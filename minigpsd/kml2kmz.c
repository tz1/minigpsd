#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>

unsigned char pklhead[] = {
    0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x02, 0x00,
    0x08, 0x00,
    // time, date next line, offset 10
    0x86, 0x6b, 0xa8, 0x36,
    0xf3, 0x46, 0x80, 0x25, 0xe3, 0x00, 0x00, 0x00,
    0x59, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char pkchead[] = {
    0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00,
    0x02, 0x00, 0x08, 0x00, 
    // time, date next line, offset 12
    0x86, 0x6b, 0xa8, 0x36,
    0xf3, 0x46, 0x80, 0x25, 0xe3, 0x00, 0x00, 0x00,
    0x59, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char pkcend[] = {
    0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x48, 0x00, 0x00, 0x00,
    0x23, 0x01, 0x00, 0x00, 0x00, 0x00
};

#define BUFMAX (16384)

unsigned char obuf[BUFMAX];

char fname[256];
char tname[256];

int main(int argc, char *argv[])
{
    pid_t pid;
    int pipeto[2];		/* pipe to feed the exec'ed program input */
    int pipefrom[2];		/* pipe to get the exec'ed program output */
    int olen, status;
    int otot = 0;
    int ifd, ofd;
    char *c, *d;

    if (argc != 2)
	return -1;

{
    unsigned int bitbuf;
    struct timeval tv;
    struct tm *tmpt;

    gettimeofday(&tv, NULL);
    tmpt = localtime(&tv.tv_sec);
    /*
    DOS stores file modification times and dates as a pair of 16-bit numbers:

        * 7 bits of year, 4 bits of month, 5 bits of day of month
        * 5 bits of hour, 6 bits of minute, 5 bits of seconds (x2). 
        */
    bitbuf = tmpt->tm_sec / 2;
    bitbuf |= tmpt->tm_min << 5;
    bitbuf |= tmpt->tm_hour << 11;

    pklhead[10] = pkchead[12] = bitbuf & 255;
    pklhead[11] = pkchead[13] = bitbuf >> 8;

    bitbuf = tmpt->tm_mday;
    bitbuf |= (1 + tmpt->tm_mon) << 5;
    bitbuf |= (tmpt->tm_year - 80) << 9;

    pklhead[12] = pkchead[14] = bitbuf & 255;
    pklhead[13] = pkchead[15] = bitbuf >> 8;

}


    nice(15);

    strcpy(fname, argv[1]);

    ifd = open(argv[1], O_RDONLY);
    if (ifd < 0)
	return -2;

    strcpy(tname, fname);
    if ((c = strstr(tname, ".kml")))
	*c = 0;
    strcat(tname, ".kmt");
    ofd = creat(tname, 0644);

    strcpy( (char *) obuf, fname);
    d = (char *) obuf;
    // strip leading path
    while ((c = strstr(d, "/")))
	d = ++c;
    strcpy(fname, d);

    write(ofd, pklhead, sizeof(pklhead));
    write(ofd, fname, strlen(fname));

    pklhead[26] = strlen(fname);
    pkchead[28] = strlen(fname);
    pkcend[12] = strlen(fname) + sizeof(pkchead);

    pipe(pipeto);
    pipe(pipefrom);
    pid = fork();
    if (!pid) {
	dup2(pipeto[0], STDIN_FILENO);
	dup2(pipefrom[1], STDOUT_FILENO);
	close(pipeto[0]);
	close(pipeto[1]);
	close(pipefrom[0]);
	close(pipefrom[1]);
	execlp("/bin/gzip", "gzip", "-9", NULL);
	exit(-1);
    }

    close(pipeto[0]);
    close(pipefrom[1]);
    if (!fork()) {
	close(pipefrom[0]);
	for (;;) {		// send infile to pipe, then exit
	    olen = read(ifd, obuf, BUFMAX);
	    //        fprintf(stderr, "read %d\n", olen);
	    if (olen > 0)
		write(pipeto[1], obuf, olen);
	    if (olen != BUFMAX) {
		close(ifd);
		close(pipeto[1]);
		return 0;
	    }
	}
    }

    close(pipeto[1]);
    read(pipefrom[0], obuf, 10);	// zap header
    read(pipefrom[0], obuf, 8);	// stay 8 behind to capture tail
    for (;;) {
	olen = read(pipefrom[0], &obuf[8], BUFMAX - 8);
	//        fprintf(stderr, "zreade %d\n", olen);
	if (olen <= 0)
	    break;
	otot += olen;
	write(ofd, obuf, olen);
	memcpy(obuf, &obuf[olen], 8);
    }

    close(pipefrom[0]);
    waitpid(-1, &status, WNOHANG);

    memcpy(&pklhead[22], &obuf[4], 4);
    memcpy(&pklhead[14], &obuf[0], 4);

    pklhead[18] = otot;
    pklhead[19] = otot >> 8;
    pklhead[20] = otot >> 16;
    pklhead[21] = otot >> 24;

    memcpy(&pkchead[6], &pklhead[4], 26);

    write(ofd, pkchead, sizeof(pkchead));
    write(ofd, fname, strlen(fname));

    // offset to CD
    otot += sizeof(pklhead);
    otot += strlen(fname);
    pkcend[16] = otot;
    pkcend[17] = otot >> 8;
    pkcend[18] = otot >> 16;
    pkcend[19] = otot >> 24;

    write(ofd, pkcend, sizeof(pkcend));

    lseek(ofd, 0, SEEK_SET);
    write(ofd, pklhead, sizeof(pklhead));
    close(ofd);
    strcpy(fname, tname);
    fname[strlen(fname) - 1] = 'z';
    rename(tname, fname);	// rename kmt to kmz
    // first insure success, then
    unlink(argv[1]);
    return 0;
}

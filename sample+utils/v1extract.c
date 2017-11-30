#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>

static char styleswitch[] =
  "</coordinates></LineString></Placemark>\n"
  "<Placemark>%s<styleUrl>#%d</styleUrl>"
  "<LineString><extrude>1</extrude>" "<altitudeMode>relativeToGround</altitudeMode>" "<coordinates>\n";

static char buf[4096];
static char whenstr[80] = "";

int main(int argc, char *argv[])
{
    int i, x, y;
    int style = 0;
    double lon, lat, spd;
    FILE *inf, *outf;
    char *c = NULL;
    int argp = 1;
    int pid, pipefrom[2];

    if (argc < 2)
        return -1;

    strcpy(buf, "Radar-");
    strcat(buf, argv[argp]);
    strcat(buf, ".kml");
    outf = fopen(buf, "w");
    if (!outf)
        return -1;

    fprintf(outf, "<kml>\n");
    fprintf(outf, "<Folder><name>Telem:%s</name>\n"
      "<Style><ListStyle><listItemType>radioFolder</listItemType>" "<bgColor>00ffffff</bgColor>" "</ListStyle></Style>\n", argv[1]);

    fprintf(outf, "<Document>\n");

    fprintf(outf, "<name>%s</name>\n", "Valentine");

    // may need to increment styles per block

    fprintf(outf,
      "<Style id=\"0\"><LineStyle><width>2</width><color>ff000000</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"1\"><LineStyle><width>2</width><color>ff336699</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"2\"><LineStyle><width>2</width><color>ff0000ff</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"3\"><LineStyle><width>2</width><color>ff0080ff</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"4\"><LineStyle><width>2</width><color>ff00ffff</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"5\"><LineStyle><width>2</width><color>ff00ff00</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"6\"><LineStyle><width>2</width><color>ffff0000</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"7\"><LineStyle><width>2</width><color>ffff00ff</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"8\"><LineStyle><width>2</width><color>ff808080</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"9\"><LineStyle><width>2</width><color>ffffffff</color>"
      "</LineStyle><PolyStyle><color>33000000</color></PolyStyle></Style>\n"
      "<Style id=\"10\"><LineStyle><width>2</width><color>ff8000ff</color>"
      "</LineStyle><PolyStyle><color>FFFFFFFF</color></PolyStyle></Style>\n");

    for (argp = 1; argp < argc; argp++) {
        fprintf(stderr, "%3d of %3d: %s\n", argp, argc, argv[argp]);

        inf = NULL;
        if (strstr(argv[argp], ".kml"))
            inf = fopen(argv[argp], "r");
        else if (strstr(argv[argp], ".kmz")) {
            pipe(pipefrom);
            pid = fork();
            if (!pid) {
                dup2(pipefrom[1], STDOUT_FILENO);
                close(pipefrom[0]);
                close(pipefrom[1]);
                execlp("/usr/bin/unzip", "unzip", "-p", argv[argp], NULL);
                fprintf(stderr, "KMZ support requires unzip to be installed\n");
                exit(-1);
            }
            close(pipefrom[1]);
            inf = fdopen(pipefrom[0], "r");
        } else {
            fprintf(stderr, "Unknown arg %s is not kml/kmz file nor \"-f speed\"\n", argv[argp]);
            continue;
        }

        if (inf == NULL)
            continue;

        waitpid(-1, &i, WNOHANG);

        // default style - 2, red for visibility
        fprintf(outf,
          "<Placemark><styleUrl>#2</styleUrl><LineString><extrude>1</extrude>"
          "<altitudeMode>relativeToGround</altitudeMode><coordinates>\n");
        style = 0;
        whenstr[0] = 0;

        fgets(buf, 4000, inf);
        while (!feof(inf)) {
            if (NULL == fgets(buf, 4000, inf))
                break;
            if (strstr(buf, "coordinates>"))
                continue;
            while (strlen(buf) && buf[strlen(buf) - 1] <= ' ')
                buf[strlen(buf) - 1] = 0;

            if (buf[0] != '<') {
                int n;

                n = sscanf(buf, "%lf,%lf,%lf", &lon, &lat, &spd);
                if (lon < 0.01 && lat < 0.01)
                    continue;
                fprintf(outf, "%s\n", buf);
                continue;
            }

            if (!strncmp(buf, "<TimeStamp><when>", 17)) {
                strcpy(whenstr, buf);
                fprintf(outf, styleswitch, whenstr, style);
            }
            if (lon < 0.01 && lat < 0.01)
                continue;
#if 0
            if (!strncmp(buf, "<!--", 4))
                fprintf(outf, "%s\n", buf);
#endif
            c = strstr(buf, "$PRDV1,");
            if (!c)
                continue;

            c += 7;
            unsigned ll, ll1;
            sscanf(c, "%08x,%08x", &ll, &ll1);
#if 0

            x = v1prev >> 1 & 1
              r = v1prev >> 5 & 1
              s = v1prev >> 8 & 1
              u = v1prev >> 11 & 1
              l = v1prev >> 12 & 1
              ka = v1prev >> 13 & 1
              k = v1prev >> 14 & 1
              p = v1prev >> 25 & 1 m = v1prev >> 31 & 1 sevseg = v1prev >> 24 & 0x7c sevseg |= v1prev >> 15 & 3;

            stren = v1prev >> 17 & 0xff
#endif
                //            fprintf(outf, "%.6f,%.6f,%d\n", lon, lat, 100);

            x = (ll >> 17) & 0xff;
            y = 0;
            while (x) {
                x >>= 1;
                y++;
            }
            if (style != y) {
                style = y > 0 ? y : 0;
                fprintf( stderr, "sig %d\n" , style );
                fprintf(outf, styleswitch, whenstr, style);
            }
        }
        fprintf(outf, "</coordinates></LineString></Placemark>\n");
        fclose(inf);
    }
    fprintf(outf, "</Document>\n");
    fprintf(outf, "</Folder></kml>\n");
    return 0;
}

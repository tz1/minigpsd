#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static char styleswitch[] =
  "</coordinates></LineString></Placemark>\n"
  "<Placemark>%s<styleUrl>#%d</styleUrl>"
  "<LineString><extrude>1</extrude>" "<altitudeMode>relativeToGround</altitudeMode>" "<coordinates>\n";

static char buf[4096];
static char whenstr[80] = "";

int main(int argc, char *argv[])
{
    int y;
    int style = 0;
    int function = 0;
    double lon, lat, spd;
    FILE *inf, *outf;
    char *c = NULL;
    int argp = 1;

    int lastfuel = 1, lastodo = 0;

    char *funcname[] = {
        "NONE", "RPM Gear", "TurnSig", "Odo Tick", "Fuel Tick", "Clutch / Neutral", "FuelTank",
        "gear", "Engine Temp", "Speedo Err", "Fuel Econ"
    };

    if (argc < 2)
        return -1;

    strcpy(buf, "Telem-");
    strcat(buf, argv[argp]);
    outf = fopen(buf, "w");
    if (!outf)
        return -1;

    fprintf(outf, "<kml>\n");
    fprintf(outf, "<Folder><name>Telem:%s</name>\n"
      "<Style><ListStyle><listItemType>radioFolder</listItemType>" "<bgColor>00ffffff</bgColor>" "</ListStyle></Style>\n", argv[1]);

    for (function = 1; function < 11; function++) {

        if (function == 7)      // use RPM for gear
            continue;
        if (function == 5)      // use RPM for clutch/neut
            continue;
        fprintf(outf, "<Document>\n");

        fprintf(outf, "<name>%s</name>\n", funcname[function]);

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
            fprintf(stderr, "%d %3d of %3d: %s\n", function, argp, argc, argv[argp]);
            inf = fopen(argv[argp], "r");
            if (!inf)
                return -2;

            // default style - 2, red for visibility
            fprintf(outf,
              "<Placemark><styleUrl>#2</styleUrl><LineString><extrude>1</extrude>"
              "<altitudeMode>relativeToGround</altitudeMode><coordinates>\n");
            style = 2; // red for vis
            whenstr[0] = 0;

            rewind(inf);
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
                    if (function != 1 && function < 7)
                        fprintf(outf, "%s\n", buf);
                    continue;
                }

                if (!strncmp(buf, "<TimeStamp><when>", 17)) {
                    strcpy(whenstr, buf);
                    fprintf(outf, styleswitch, whenstr, style);
                }
#if 0
                if (!strncmp(buf, "<!--", 4))
                    fprintf(outf, "%s\n", buf);
#endif
                c = strstr(buf, "$PD");
                if (!c)
                    continue;
                c += 3;
                //      fprintf( stderr, "%3.3s=%4.4s=%8.8s\n", &buf[10], &buf[13], &c[4] );
                if (lon < 0.01 && lat < 0.01)
                    continue;

                y = atoi(&c[4]);

                switch (function) {
                case 0:
                    break;
                case 1:
                    if (!strncmp(c, "RPM", 3))
                        fprintf(outf, "%.6f,%.6f,%d\n", lon, lat, y / 50);
                    if (!strncmp(c, "GER", 3) && y > 0 && style != y) {
                        style = y > 0 ? y : 0;
                        fprintf(outf, styleswitch, whenstr, style);
                    }
                    if (!strncmp(c, "CLU", 3)) {
                        switch (strtoul(&c[16], NULL, 16)) {
                        case 0x02:
                            // NONE - powered
                            if (style == 0)
                                style = 8;      //no clutch, gear ?
                            break;
                        case 0x82:     //clutch
                            style = 0;
                            break;
                        case 0x20:
                            style = 9;  //neutral
                            break;
                        case 0xA0:
                            style = 7;  //neutral/clutch
                            break;
                        default:
                            style = 10;
                            break;
                        }
                        fprintf(outf, styleswitch, whenstr, style);
                    }

                    break;
                case 2:
                    if (!strncmp(c, "SGN", 3)) {
                        style = (3 & c[19]) * 2;
                        fprintf(outf, styleswitch, whenstr, style);
                    }
                    break;
                case 3:
                    if (!strncmp(c, "ODO", 3)) {
                        style = !style * 2;
                        fprintf(outf, styleswitch, whenstr, style);
                    }
                    break;
                case 4:
                    if (!strncmp(c, "FUL", 3)) {
                        style = !style * 2;
                        fprintf(outf, styleswitch, whenstr, style);
                    }
                    break;
                case 5:
                    if (!strncmp(c, "CLU", 3)) {
                        switch (strtoul(&c[16], NULL, 16)) {
                        case 0x02:
                            style = 2;
                            break;
                        case 0x82:
                            style = 0;
                            break;
                        case 0x20:
                            style = 6;
                            break;
                        case 0xA0:
                            style = 5;
                            break;
                        default:
                            style = 7;
                            break;
                        }
                        fprintf(outf, styleswitch, whenstr, style);
                    }
                    break;
                case 6:
                    if (!strncmp(c, "GAS", 3) && y != style) {
                        style = y;
                        fprintf(outf, styleswitch, whenstr, style);
                    }
                    break;
                case 7:
                    if (!strncmp(c, "GER", 3) && style != y) {
                        style = y;
                        fprintf(outf, styleswitch, whenstr, style);
                    }
                    break;
                case 8:
                    if (!strncmp(c, "HOT", 3)) {
                        fprintf(outf, "%.6f,%.6f,%d\n", lon, lat, y);
                    }
                    break;
                case 9:
                    if (!strncmp(c, "SPD", 3)) {
                        double rat;
                        rat = atof(&c[4]);
                        if (spd > 0.1)
                            rat /= spd;
                        else
                            rat = 0.0;
                        fprintf(outf, "%.6f,%.6f,%.3f\n", lon, lat, rat * 100);
                        y = (int) spd;
                        //y += 5;
                        y /= 10;
                        y %= 10;
                        if (style != y) {
                            style = y;
                            fprintf(outf, styleswitch, whenstr, style);
                            fprintf(outf, "%.6f,%.6f,%.3f\n", lon, lat, rat * 100);
                        }
                    }
                    break;
                case 10:
                    if (!strncmp(c, "ODO", 3)) {
                        c = strchr(&c[4], ',');
                        c++;
                        lastodo = atoi(c);
                        if (y && y != 100)
                            lastodo = lastodo * 100 / y;
                        y = 4 * 25 * lastodo / lastfuel;
                        if (y > 1000)
                            y = 1000;
                        fprintf(outf, "%.6f,%.6f,%d\n", lon, lat, y);
                    }
                    if (!strncmp(c, "FUL", 3)) {
                        c = strchr(&c[4], ',');
                        c++;
                        lastfuel = atoi(c);
                        if (y && y != 20)
                            lastfuel = lastfuel * 20 / y;
                        if (!lastfuel)
                            lastfuel = 1;
                        y = 4 * 25 * lastodo / lastfuel;
                        if (y > 1000)
                            y = 1000;
                        fprintf(outf, "%.6f,%.6f,%d\n", lon, lat, y);
                    }
                    break;
                }
            }
            fprintf(outf, "</coordinates></LineString></Placemark>\n");
            fclose(inf);
        }
        fprintf(outf, "</Document>\n");
    }
    fprintf(outf, "</Folder></kml>\n");
    return 0;
}

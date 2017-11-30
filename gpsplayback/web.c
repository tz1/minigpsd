#include "minigpsd.h"

static char rtname[] = "miniGPSD Playback";

// web pages from server

//kml feed

static char gpskml[] = "<Document>"
  "<flyToView>1</flyToView>"
  "<name>%s %d.%03d</name>"
  "<LookAt>"
  "<longitude>%d.%06d</longitude>\n"
  "<latitude>%d.%06d</latitude>\n"
  "<range>%d</range>"
  "<tilt>%d</tilt>"
  "<heading>%d.%03d</heading>"
  "</LookAt>"
  "<Placemark>"
  "<Style><IconStyle><Icon><href>"
  "root://icons/high_res_places.png"
  "</href></Icon></IconStyle></Style>"
  "<Point><coordinates>" "%d.%06d,%d.%06d" "</coordinates></Point>" "</Placemark>" "</Document>\n";

void dokml(char *c)
{
    char *d;
    int range = 1500, tilt = 45;
    d = strstr(c, "kml");
    d--;
    d--;
    while (*d && (*d >= '0' && *d <= '9'))
        d--;
    range = atoi(&d[1]);
    d--;
    while (*d && (*d >= '0' && *d <= '9'))
        d--;

    tilt = atoi(&d[1]);

    sprintf(xbuf, gpskml, rtname, gpst.gspd / 1000, gpst.gspd % 1000,
      gpst.llon / 1000000, abs(gpst.llon % 1000000), gpst.llat / 1000000, abs(gpst.llat % 1000000), range, tilt, gpst.gtrk / 1000, gpst.gtrk % 1000,
      gpst.llon / 1000000, abs(gpst.llon % 1000000), gpst.llat / 1000000, abs(gpst.llat % 1000000));
}

//moving google map

static char xmldata[] = "<xml><markers><marker lat=\"%d.%06d\" lng=\"%d.%06d\"/></markers></xml>";

void doxml()
{
    sprintf(xbuf, xmldata, gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.llon / 1000000, abs(gpst.llon % 1000000));
}

static char dogmaphtml[] =
  "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">"
  "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:v=\"urn:schemas-microsoft-com:vml\">"
  "<head><title>MiniGPSD Google Maps</title>"
  "<script src=\"http://maps.google.com/maps?file=api&v=2&key="
  "ABQIAAAANuLIexvN57fLejioS_Sc1xSiW6_TZA8KXStWCUXuIknmHbpzIxQde8r5eV8EKfChVtmY49dFftroVg"
  ///*localhost:8888 */ "ABQIAAAANuLIexvN57fLejioS_Sc1xSiW6_TZA8KXStWCUXuIknmHbpzIxQde8r5eV8EKfChVtmY49dFftroVg"
  ///*127.0.0.1:8888*/ "ABQIAAAANuLIexvN57fLejioS_Sc1xRi_j0U6kJrkFvY4-OX2XYmEAa76BQr5MmGpSXMqbjSSeNcvxvHyxx6_Q"
  "&sensor=false"
  "\" type=\"text/javascript\"></script>"
  "<script type=\"text/javascript\"> var map; var mark; var timer = setInterval(\"posrefresh()\", 5 * 1000 );"
  "function posrefresh(){"
  "GDownloadUrl(\"gpsdata.xml\",function(data, responseCode) {"
  "var xml = GXml.parse(data); var markers = xml.documentElement.getElementsByTagName(\"marker\");var point;"
  "for(var i = 0; i < markers.length; i++){point = new GLatLng( parseFloat(markers[i].getAttribute(\"lat\")),"
  " parseFloat(markers[i].getAttribute(\"lng\")));}mark.setLatLng(point);map.panTo(point); });}"
  "function load(){if(GBrowserIsCompatible()){map = new GMap2(document.getElementById(\"map\"));"
  "	map.checkResize();"
  "mark = new GMarker(new GLatLng(0.0,0.0));"
  "map.setCenter(new GLatLng(44.0,102.25), 14);map.addControl(new GMapTypeControl());"
  "var topts = {incidents:true}; var tinfo = new GTrafficOverlay(topts);map.addOverlay(tinfo);"

  "map.addControl(new GSmallZoomControl());map.addControl(new GScaleControl());posrefresh();map.addOverlay(mark);}}"
  "</script></head><body>"
"<table><tr><td><a href=/><H1><br>"
"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
"<br>Back<br>"
"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
"</a><td>"
  "<div id=\"map\" style=\"width: 360px; height:360px \"></div>"
  "<script>window.onload = load;window.onunload = GUnload;</script>" 
"</table></body></html>";


void dogmap()
{
    strcpy(xbuf, dogmaphtml);
}

static char radfmt[] = 
"<html><head><title>WeatherRadar</title><meta http-equiv=\"refresh\" content=\"60\"></head><body><table><tr><td><a href=/><h1>"
"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
"<br>BACK<br>"
"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
"</a><td width=360 height=360>"
"<IFRAME align=right width=100%% height=100%% src="
"\"http://radblast-mi.wunderground.com/cgi-bin/radar/WUNIDS_composite?centerlat=%d.%06d&centerlon=%d.%06d&radius=%d"
"&type=N0R&frame=0&num=5&delay=10&width=340&height=340&newmaps=1&r=1159834180&showstorms=0&theme=WUNIDS_severe&rainsnow=1\""
"></IFRAME></body></html>\n";

void dorad(int size)
{
   sprintf(xbuf, radfmt, gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.llon / 1000000, abs(gpst.llon % 1000000), size);
}

//standard gps view

static char gpspage1[] = "<HEAD><TITLE>%s</TITLE><meta http-equiv=\"refresh\" content=\"5\"></HEAD><BODY>";

static char gpspage2[] =
  "<table><tr><td>"
  "<table border=1><tr><td><a href=/dogmap.html><h1>"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "<br>&nbsp;&nbsp;MAP&nbsp;&nbsp;<br>"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "</a></tr>"
  "<tr><td><a href=/radar20.html><h1>"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "<br>RADAR<br>"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
  "</a></tr></table>"
  "<td><table border=1><tr><td>"
  "%02d-%02d-%02d %02d:%02d:%02d<tr><td>"
  "lat=%d.%06d<tr><td>lon=%d.%06d<tr><td>"
  "%dD fix<tr><td>%s lock<tr><td>"
  "alt=%d.%03d<tr><td>"
  "spd=%d.%03d<tr><td>"
  "trk=%d.%03d<tr><td>" "PDoP=%d.%02d<tr><td>" "HDoP=%d.%02d<tr><td>" "VDoP=%d.%02d</table>\n"
  "<td><table border=1><tr><th>SN<th>EL<th>AZM<th>SG<th>U</tr>";

static char gpspage9[] = "</table></table></BODY></HTML>\r\n\r\n";

void doweb()
{
    char *c;
    int n;
    c = strchr(xbuf, '&');
    if (!c)
        c = "";
    sprintf(xbuf, gpspage1, rtname);
    sprintf(&xbuf[strlen(xbuf)], gpspage2, gpst.yr, gpst.mo, gpst.dy, gpst.hr, gpst.mn, gpst.sc, gpst.llat / 1000000, abs(gpst.llat % 1000000), gpst.llon / 1000000,
      abs(gpst.llon % 1000000), gpst.fix, gpst.lock ? (gpst.lock == 1 ? "GPS" : "DGPS") : "no", gpst.alt / 1000, abs(gpst.alt % 1000)
      , gpst.gspd / 1000, gpst.gspd % 1000, gpst.gtrk / 1000, gpst.gtrk % 1000, gpst.pdop / 1000, gpst.pdop % 1000 / 10, gpst.hdop / 1000, gpst.hdop % 1000 / 10,
      gpst.vdop / 1000, gpst.vdop % 1000 / 10);
    for (n = 0; n < gpsat.nsats; n++) {
        int m;
        sprintf(&xbuf[strlen(xbuf)], "<tr><td>%02d<td>%02d<td>%03d<td>%02d<td>", gpsat.view[n], gpsat.el[gpsat.view[n]], gpsat.az[gpsat.view[n]], gpsat.sn[gpsat.view[n]]);
        for (m = 0; m < 12; m++)
            if (gpsat.sats[m] == gpsat.view[n])
                break;
        sprintf(&xbuf[strlen(xbuf)], "%s</tr>", m == 12 ? " " : "*");
    }
    strcpy(&xbuf[strlen(xbuf)], gpspage9);
}

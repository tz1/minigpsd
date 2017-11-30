#!/usr/bin/env python

import gtk
import gobject
import bluetooth
import os
import time
import struct
import mmap
import copy

try:
    import gnome.gconf as gconf
except ImportError:
    import gconf

try:
    import hildon
except ImportError:
    hildon = None

import dbus, dbus.service
from dbus.mainloop.glib import DBusGMainLoop

class MGSetup():
    cgclnt = None
    iconupdate = 3000
    invokedfrom = 0
    __gcpath = '/apps/maemo/minigpsd'
    gpstfile = None
    gpstdata = None

    #exported
    button = None
    menu = None

    def setup(self,cfrom):

        self.invokedfrom = cfrom
        self.button = gtk.Button()
        self.button.set_image(gtk.image_new_from_stock(gtk.STOCK_YES, gtk.ICON_SIZE_MENU))
        self.button.set_name("menu")
        if self.invokedfrom == 1:
            self.button.set_size_request(40,40)
        elif self.invokedfrom == 2:
            self.button.set_size_request(64,64)
	    gtk.Widget.set_name( self.button, "hildon-navigator-button-one" )
        else:
            self.button.set_size_request(320,320)
        self.__create_menu()
        if self.invokedfrom != 3: ###
            self.button.connect("clicked",self.__popup_menu)
        self.button.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(24000,24000,24000))
        self.button.modify_bg(gtk.STATE_PRELIGHT, gtk.gdk.Color(32000,32000,32000))
        self.button.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.Color(40000,40000,40000))
        self.button.show()
        gobject.timeout_add(500,self.__updateicon)

        self.cgclnt = gconf.client_get_default()
        val = self.__getgconf( "ICONUPDATE" )
        if not val:
            val = 3
        self.iconupdate = int(val)

        if not os.path.exists("/tmp/mgpstate"):
            self.gpstfile = open("/tmp/mgpstate", "w+")
            t = "";
            for i in range(244):
                t+="\0"
            self.gpstfile.write(t)
            self.gpstfile.close()
            
        self.gpstfile = open("/tmp/mgpstate", "r+")
        self.gpstdata = mmap.mmap(self.gpstfile.fileno(), 244)

        self.cgclnt.add_dir( self.__gcpath , gconf.CLIENT_PRELOAD_NONE)

        if not cfrom:
            pass

        self.m_loop = DBusGMainLoop()
        self.bus = dbus.SystemBus(mainloop = self.m_loop, private = True)
        self.handler = self.bus.add_signal_receiver(self._inactivity_handler,
                                                    signal_name="display_status_ind",
                                                    dbus_interface='com.nokia.mce.signal',
                                                    path='/com/nokia/mce/signal')

        return self.button

    active = 1;

    def _inactivity_handler(self, inact=None):
        if inact == "off":
            self.active = 0
        else:
            if self.active == 0:
                gobject.timeout_add(100,self.__updateicon)
            self.active = 1

    statxpm = [
"39 39 16 1",
". c None",
"a c #000000",
"b c #606060",
"# c #ffffff",
"x c None",
"y c None",
"0 c #ff0000",
"1 c #ffa000",
"2 c #ffd000",
"3 c #ffff00",
"4 c #80ff00",
"5 c #00ff00",
"6 c #c0ffa0",
"7 c #ffffff",
"8 c #ff00ff",
"9 c #008080",

"..xx...........#########...............",
".xxxx.......###############............",
"xxxxxx....#####aaaabaaaa#####..........",
"xxxxxx...###aaaaaaabaaaaaaa###.........",
".xxxx..###aaaaaaaaabaaaaaaaaa###.......",
"..xx..###aaaaaaaaaabaaaaaaaaaa###......",
".....###aaaaaaaaaaabaaaaaaaaaaa###.....",
"....###aaaaaaaaabbbbbbbaaaaaaaaa###....",
"....##aaaaaaaabbaaabaaabbaaaaaaaa##....",
"...##aaaaaaabbaaaaabaaaaabbaaaaaaa##...",
"..##aaaaaaabaaaaaaabaaaaaaabaaaaaaa##..",
"..##aaaaaabaaaaaaaabaaaaaaaabaaaaaa##..",
".##aaaaaabaaaaaaaaabaaaaaaaaabaaaaaa##.",
".##aaaaaabaaaaaaabbbbbaaaaaaabaaaaaa##.",
".##aaaaabaaaaaaabaabaabaaaaaaabaaaaa##.",
"##aaaaaabaaaaaabaaabaaabaaaaaabaaaaaa##",
"##aaaaabaaaaaabaaaabaaaabaaaaaabaaaaa##",
"##aaaaabaaaaabaaaaabaaaaabaaaaabaaaaa##",
"##aaaaabaaaaabaaaaabaaaaabaaaaabaaaaa##",
"##bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb##",
"##aaaaabaaaaabaaaaabaaaaabaaaaabaaaaa##",
"##aaaaabaaaaabaaaaabaaaaabaaaaabaaaaa##",
"##aaaaabaaaaaabaaaabaaaabaaaaaabaaaaa##",
"##aaaaaabaaaaaabaaabaaabaaaaaabaaaaaa##",
".##aaaaabaaaaaaabaabaabaaaaaaabaaaaa##.",
".##aaaaaabaaaaaaabbbbbaaaaaaabaaaaaa##.",
".##aaaaaabaaaaaaaaabaaaaaaaaabaaaaaa##.",
"..##aaaaaabaaaaaaaabaaaaaaaabaaaaaa##..",
"..##aaaaaaabaaaaaaabaaaaaaabaaaaaaa##..",
"...##aaaaaaabbaaaaabaaaaabbaaaaaaa##...",
"....##aaaaaaaabbaaabaaabbaaaaaaaa##....",
"....###aaaaaaaaabbbbbbbaaaaaaaaa###....",
".....###aaaaaaaaaaabaaaaaaaaaaa###.....",
"..yy..###aaaaaaaaaabaaaaaaaaaa###......",
".yyyy..###aaaaaaaaabaaaaaaaaa###.......",
"yyyyyy...###aaaaaaabaaaaaaa###.........",
"yyyyyy....#####aaaabaaaa#####..........",
".yyyy.......###############............",
"..yy...........#########..............."
               ]

    numstr="0123456789"

    sinetab = [ 0, 571, 1143, 1714, 2285, 2855, 3425, 3993, 4560, 5126, 5690, 6252, 6812,
                7371, 7927, 8480, 9032, 9580, 10125, 10668, 11207, 11743, 12275, 12803,
                13327, 13848, 14364, 14876, 15383, 15886, 16383, 16876, 17364, 17846, 18323,
                18794, 19260, 19720, 20173, 20621, 21062, 21497, 21926, 22347, 22762, 23170,
                23571, 23964, 24351, 24730, 25101, 25465, 25821, 26169, 26509, 26841, 27165,
                27481, 27788, 28087, 28377, 28659, 28932, 29196, 29451, 29697, 29935, 30163,
                30381, 30591, 30791, 30982, 31164, 31336, 31498, 31651, 31794, 31928, 32051,
                32165, 32270, 32364, 32449, 32523, 32588, 32643, 32688, 32723, 32748, 32763,
                32768 ]
                
#	def icos(self, angle): return isin(angle + 90)

    def isin(self, angle):
        angle %= 360
        if angle <= 90:
            return self.sinetab[angle]
        elif angle <= 180:
            return self.sinetab[180 - angle]
        elif angle <= 270:
            return -self.sinetab[angle - 180]
        else:
            return -self.sinetab[360 - angle]

    def aetoxy( self, azim, elev, radius ):
        rad2 = (90 - elev) * radius
        tlx = radius + self.isin(azim) * rad2 / (32768*90)
        tly = radius - self.isin(90 - azim) * rad2 / (32768*90)
        return (tlx,tly)
               
    def __updateicon(self):
        gpstate = struct.unpack("21i80h",self.gpstdata)
        xpm = copy.copy(self.statxpm)

        if os.path.exists("/tmp/minigpsd.pid"):

            if gpstate[15] == 2:
                xpm[4] = "# c #0000ff"
            elif gpstate[16] == 3:
                xpm[4] = "# c #00ff00"
            elif gpstate[16] == 2:
                xpm[4] = "# c #ffff00"
            else:
                xpm[4] = "# c #ff0000"

#Technnically, should be >= 0, but when inited they would show as active
            if gpstate[17] > 0:
                xpm[5] = "x c #ffffff"
            else:
                xpm[5] = "x c None"
                
            if gpstate[18] > 0:
                xpm[6] = "y c #ffffff"
            else:
                xpm[6] = "y c None"

            for i in range(gpstate[19]): #number of satellites
                j=21+i*4

                x,y = self.aetoxy(gpstate[j+2],gpstate[j+1],18)
                s = self.numstr[gpstate[j+3]/8]

                t = xpm[18+y]
                if gpstate[j] > 33 or gpstate[j] < -33:
                    t = t[0:x] + s + "a" + s + t[3+x:]
                else:
                    t = t[0:x] + s + s + s + t[3+x:]
                xpm[18+y] = t
                t = xpm[17+y]
                t2 = xpm[19+y]
                if gpstate[j] < 0 : #used - fat dot
                    t = t[0:x] + s+s+s + t[3+x:]
                    t2 = t2[0:x] + s+s+s + t2[3+x:]
                else: #not used - plus
                    t = t[0:x+1] + s + t[2+x:]
                    t2 = t2[0:x+1] + s + t2[2+x:]
                xpm[17+y] = t
                xpm[19+y] = t2
#end if minigpsd active : satlist plot

        pixbuf = gtk.gdk.pixbuf_new_from_xpm_data(xpm)
        image = gtk.Image()
	if self.invokedfrom > 0:
            image.set_from_pixbuf(pixbuf)
        else:
            w = self.button.get_allocation().width
            h = self.button.get_allocation().height
            if w > h:
                w = h
            image.set_from_pixbuf(pixbuf.scale_simple(w,w,gtk.gdk.INTERP_NEAREST))
        self.button.set_image(image)
        if self.active:
            gobject.timeout_add(self.iconupdate * 1000,self.__updateicon)
#debug
        else:
            self.button.set_image(gtk.image_new_from_stock(gtk.STOCK_YES, gtk.ICON_SIZE_MENU))            


    act_menu_item = None
    def __create_menu(self):
        self.menu = gtk.Menu()
#ImageMenuItem to get right theme for taskbar
        self.act_menu_item = gtk.ImageMenuItem("GPSD Active")
        self.act_menu_item.connect('activate',self.__togglegpsd)
        self.menu.append(self.act_menu_item)

        menu_item = gtk.ImageMenuItem("Configure...")
        self.menu.append(menu_item)
        menu_item.connect('activate',self.__gpsddialog)

	if self.invokedfrom == 2:
	    gtk.Widget.set_name( self.menu, "menu_from_navigator" )
            gtk.Widget.set_size_request( self.act_menu_item, 240,64 )
            gtk.Widget.set_size_request( menu_item, 240,64 )

        menu_item = gtk.ImageMenuItem("Dashboard")
        menu_item.connect('activate',self.__launchdash)
        self.menu.append(menu_item)

	if self.invokedfrom == 2:
	    gtk.Widget.set_name( self.menu, "menu_from_navigator" )
            gtk.Widget.set_size_request( self.act_menu_item, 240,64 )
            gtk.Widget.set_size_request( menu_item, 240,64 )

        self.menu.show_all()

    def __popup_menu(self,widget,data=None):
        t = self.act_menu_item.get_children()[0]
        if os.path.exists("/tmp/minigpsd.pid"):
            t.set_label("Stop GPSD")

        else:
            t.set_label("Start GPSD")
        self.menu.popup(None,None,self.__menu_position,0,gtk.get_current_event_time())

    def __menu_position(self,data=None):
        if self.invokedfrom == 2:
            x = self.button.get_allocation().x + self.button.get_allocation().width
            y = self.button.get_allocation().y
            return (x,y,True)
        (reqw, reqh) = self.menu.get_size_request()
        sw = self.button.get_screen().get_width()
        (x,y) = self.button.get_parent_window().get_position()
        if self.invokedfrom == 1:
            y = y + self.button.get_allocation().y + self.button.get_allocation().height
            x = x + self.button.get_allocation().x
        greater = x + reqw
        if greater > sw:
            x = x - req.width - self.button.get_allocation().width;
        return (x,y,True)

    def __togglegpsd(self,widget,data=None):
        if os.path.exists("/tmp/minigpsd.pid"):
            os.system( "killall minigpsd" )
            os.system( "killall miniobd2d" )
            os.system( "killall miniv1d" )
            os.system( "rm /tmp/minigpsd.pid" ) #to prevent stale file from locking us
        else:
            os.system( "/usr/bin/minigpsd &" )
            t = self.__getgconf( "USEOBD" )
            if t == "1":
                t = self.__getgconf( "USEHD" )
                if t != "1":
                    os.system( "/usr/bin/miniobd2d &" )
            t = self.__getgconf( "USEVV1" )
            if t == "1":
                os.system( "/usr/bin/miniv1d &" )

    def __launchdash(self,widget,data=None):
        t = self.__getgconf( "USEOBD" )
        if t == "1":
            os.system( "/usr/bin/obd2dash.py &" )
            return;
        os.system( "/usr/bin/harleydash.py &" )
        return;

    def __setgconf(self, key, val):
        self.cgclnt.set_string(self.__gcpath + "/" + key, val )

    def __getgconf(self, key):
        return self.cgclnt.get_string(self.__gcpath + "/" + key)

    def __gpsddialog(self, widget):
        dialog = gtk.MessageDialog( None,
            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            gtk.MESSAGE_QUESTION, gtk.BUTTONS_OK_CANCEL, None)

        dialog.set_markup("Configure MiniGPSD operation")

        lwid = 12
        if os.path.exists("/var/lib/gps/gps_driver_ctrl"):
            hbox = gtk.HBox(False, 0 )
            dialog.vbox.pack_start(hbox, True,True, 0)
            label = gtk.Label("Internal GPS");
            hbox.pack_start(label, False , True, 0)
            label.set_width_chars(lwid);
            internal = gtk.CheckButton("Use n810");
            t = self.__getgconf( "GPSI" )
            if t == "Internal":
                internal.set_active(1);
            hbox.pack_start(internal, True , True, 0)



        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("BT set dev to /dev/rfcommN N=0..9");
        hbox.pack_start(label, False , True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("External GPS");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)

        gpsenable = gtk.CheckButton("On");
        t = self.__getgconf( "USEGPS" )
        if t == "1":
            gpsenable.set_active(1);
        hbox.pack_start(gpsenable, True , True, 0)

        label = gtk.Label("Device:");
        hbox.pack_start(label, False , True, 0)

        externdev = gtk.Entry()
        externdev.set_max_length(30)
        externdev.set_width_chars(15)
        t = self.__getgconf( "GPSdev" )
        if not t:
            t = "/dev/rfcomm0"
        externdev.set_text(t)
        hbox.pack_start(externdev, False, True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("New KML Every");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , False, 0)
        val = self.__getgconf( "KMLINTERVAL" )
        if not val:
            val = 0
        adj = gtk.Adjustment(float(val), 0, 60*24, 1, 30, 0)
        kmlrate = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(kmlrate, False , False, 0)
        label = gtk.Label("Min. 0-nolog ");
        hbox.pack_start(label, False , False, 0)

        button = gtk.Button()
        button.set_label("to Folder...")
        button.connect("clicked",self.__kmldir)
        hbox.pack_start(button, True,True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Web Title");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)
        webtitle = gtk.Entry()
        webtitle.set_max_length(80)
        webtitle.set_width_chars(32)
        t = self.__getgconf( "MYNAME" )
        if not t:
            t = "MiniGPSD"
        webtitle.set_text(t)
        hbox.pack_start(webtitle, False , True, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)

        button = gtk.Button()
        button.set_label("GPSGate...")
        button.connect("clicked",self.__gatedialog)
        hbox.pack_start(button, True,False, 0)

        button = gtk.Button()
        button.set_label("Advanced...")
        button.connect("clicked",self.__advanced)
        hbox.pack_start(button, True,False, 0)


        button = gtk.Button()
        button.set_label("BT Scan...")
        button.connect("clicked",self.__btscan)
        hbox.pack_start(button, True,False, 0)


        dialog.show_all()
        t = dialog.run()
        if t == gtk.RESPONSE_OK:
            val = kmlrate.get_value_as_int()
            if not os.path.exists("/var/lib/gps/gps_driver_ctrl"):
                self.__setgconf( "GPSI", "OFF" )
            elif internal.get_active():
                self.__setgconf( "GPSI", "Internal" );
            else:
                self.__setgconf( "GPSI", "OFF" )
            if gpsenable.get_active():
                self.__setgconf( "USEGPS", "1" );
            else:
                self.__setgconf( "USEGPS", "0" );
            self.__setgconf( "KMLINTERVAL", str(val) )
            self.__setgconf( "MYNAME", webtitle.get_text() )
            self.__setgconf( "GPSdev", externdev.get_text() )

        dialog.destroy()


    __btcomblist = None
    __gpsaddr = None
    __obdaddr = None
    __vv1addr = None

    def __btsetgps(self,widget):
        ml = self.__btcomblist.get_model()
        ix = self.__btcomblist.get_active()
        t = ml[ix][0][0:17]
        self.__gpsaddr.set_text(t)
        self.__setgconf( "GPSaddr", t )

    def __btsetobd(self,widget):
        ml = self.__btcomblist.get_model()
        ix = self.__btcomblist.get_active()
        t = ml[ix][0][0:17]
        self.__obdaddr.set_text(t)
        self.__setgconf( "OBDaddr", t )

    def __btsetvv1(self,widget):
        ml = self.__btcomblist.get_model()
        ix = self.__btcomblist.get_active()
        t = ml[ix][0][0:17]
        self.__vv1addr.set_text(t)
        self.__setgconf( "VV1addr", t )

    __btstat = None
    def __btdoscan(self):
        try:
            btlist = bluetooth.discover_devices(lookup_names=True)
        except bluetooth.BluetoothError:
            btlist = None

        if btlist:
            for addr,name in btlist:
                self.__btcomblist.append_text(addr+"   |   "+name)

        self.__btstat.set_text("Bluetooth Device Selections:");


    def __btscan(self,widget):
        dialog = gtk.MessageDialog( None,
            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            gtk.MESSAGE_QUESTION, gtk.BUTTONS_OK, None)

        dialog.set_markup("Bluetooth Scan and Set")

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Use BT settings to pair and set trusted first")
        hbox.pack_start(label, False , True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Use selected value from scan or type in address")
        hbox.pack_start(label, False , True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        self.__btstat = gtk.Label("Bluetooth Device List: (*SEARCHING*)");
        hbox.pack_start(self.__btstat, False , True, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)


        self.__btcomblist = gtk.combo_box_new_text()
        t1 = self.__getgconf( "GPSaddr" )
        if t1:
    	    t = t1 + "   |   Current GPS Saved"
        else:
            t = "00:00:00:00:00:00   |   No GPS Set"
        self.__btcomblist.append_text(t)
        try:
            f = open("/var/lib/gps/gps_BT_devices","r")
            t1 = f.readline()
            t = t1[0:17] + "   |   Location Manager"
            f.close()
            self.__btcomblist.append_text(t)
        except IOError:
            pass

        t1 = self.__getgconf( "OBDaddr" )
        if t1:
            t = t1 + "   |   Current OBD Saved"
        else:
            t = "00:00:00:00:00:00   |   No OBD Set"
        self.__btcomblist.append_text(t)

        t1 = self.__getgconf( "VV1addr" )
        if t1:
            t = t1 + "   |   Current V1 Saved"
        else:
            t = "00:00:00:00:00:00   |   No V1 Set"
        self.__btcomblist.append_text(t)
        self.__btcomblist.set_active(0)
        hbox.pack_start(self.__btcomblist, False, True, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)

        l1 = gtk.Label("GPS BT Addr:");
        hbox.pack_start(l1, False , True, 0)

        self.__gpsaddr = gtk.Entry()
        self.__gpsaddr.set_max_length(17)
        self.__gpsaddr.set_width_chars(17)
        t = self.__getgconf( "GPSaddr" )
        if not t:
            t = "00:00:00:00:00:00"
        self.__gpsaddr.set_text(t)
        hbox.pack_start(self.__gpsaddr , True,False, 0)

        button = gtk.Button()
        button.set_label("Set GPS")
        button.connect("clicked",self.__btsetgps)
        hbox.pack_start(button, True,False, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)

        l1 = gtk.Label("OBD BT Addr:");
        hbox.pack_start(l1, False , True, 0)

        self.__obdaddr = gtk.Entry()
        self.__obdaddr.set_max_length(17)
        self.__obdaddr.set_width_chars(17)
        t = self.__getgconf( "OBDaddr" )
        if not t:
            t = "00:00:00:00:00:00"
        self.__obdaddr.set_text(t)

        hbox.pack_start(self.__obdaddr , True,False, 0)

        button = gtk.Button()
        button.set_label("Set OBD")
        button.connect("clicked",self.__btsetobd)
        hbox.pack_start(button, True,False, 0)

        
        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)

        l1 = gtk.Label("V1 BT Addr:");
        hbox.pack_start(l1, False , True, 0)

        self.__vv1addr = gtk.Entry()
        self.__vv1addr.set_max_length(17)
        self.__vv1addr.set_width_chars(17)
        t = self.__getgconf( "VV1addr" )
        if not t:
            t = "00:00:00:00:00:00"
        self.__vv1addr.set_text(t)

        hbox.pack_start(self.__vv1addr , True,False, 0)

        button = gtk.Button()
        button.set_label("Set V1")
        button.connect("clicked",self.__btsetvv1)
        hbox.pack_start(button, True,False, 0)

        dialog.show_all()
        gobject.timeout_add(500,self.__btdoscan)
        t = dialog.run()

        t = self.__gpsaddr.get_text()
        self.__setgconf( "GPSaddr", t )
        t = self.__obdaddr.get_text()
        self.__setgconf( "OBDaddr", t )
        t = self.__vv1addr.get_text()
        self.__setgconf( "VV1addr", t )

        dialog.destroy()


    def __kmldir(self,widget):
        if False and hildon:
            fcd = hildon.FileChooserDialog(widget.get_parent_window(),#window,
                        gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER)
        else:
            fcd = gtk.FileChooserDialog("Select Directory for KMZ",
                                    None,
                                    gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                    (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                                     gtk.STOCK_OPEN, gtk.RESPONSE_OK))

        fcd.set_default_response(gtk.RESPONSE_OK)
        t = self.__getgconf( "LOGDIR" )
        if t:
            fcd.set_current_folder(t)
        response = fcd.run()
        if response == gtk.RESPONSE_OK:
            self.__setgconf( "LOGDIR", fcd.get_filename() )
        fcd.destroy()

    def __advanced(self,widget):
        dialog = gtk.MessageDialog( None,
            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            gtk.MESSAGE_QUESTION, gtk.BUTTONS_OK_CANCEL, None)

        dialog.set_markup("Advanced GPS Setup")

        lwid = 10


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("BT set dev to /dev/rfcommN N=0..9");
        hbox.pack_start(label, False , True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("OBD");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)

        obdenable = gtk.CheckButton("On");
        t = self.__getgconf( "USEOBD" )
        if t == "1":
            obdenable.set_active(1);
        hbox.pack_start(obdenable, True , True, 0)

        hdenable = gtk.CheckButton("HD");
        t = self.__getgconf( "USEHD" )
        if t == "1":
            hdenable.set_active(1);
        hbox.pack_start(hdenable, True , True, 0)

        label = gtk.Label("Device:");
        hbox.pack_start(label, False , True, 0)
        obddev = gtk.Entry()
        obddev.set_max_length(30)
        obddev.set_width_chars(15)
        t = self.__getgconf( "OBDdev" )
        if not t:
            t = "/dev/rfcomm1"
        obddev.set_text(t)
        hbox.pack_start(obddev, False, True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("V1");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)
        vv1enable = gtk.CheckButton("Enable");
        t = self.__getgconf( "USEVV1" )
        if t == "1":
            vv1enable.set_active(1);
        hbox.pack_start(vv1enable, True , True, 0)
        label = gtk.Label("Device:");
        hbox.pack_start(label, False , True, 0)
        vv1dev = gtk.Entry()
        vv1dev.set_max_length(30)
        vv1dev.set_width_chars(15)
        t = self.__getgconf( "VV1dev" )
        if not t:
            t = "/dev/rfcomm2"
        vv1dev.set_text(t)
        hbox.pack_start(vv1dev, False, True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("http port");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , False, 0)
        val = self.__getgconf( "HTTPPORT" )
        if not val:
            val = 8888
        adj = gtk.Adjustment(float(val), 1024, 65535, 1, 256, 0)
        httpval = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(httpval, False , False, 0)

        sep = gtk.HSeparator()
        hbox.pack_start(sep, True, False, 0)

        label = gtk.Label("gpsd port");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , False, 0)
        val = self.__getgconf( "GPSDPORT" )
        if not val:
            val = 2947
        adj = gtk.Adjustment(float(val), 1024, 65535, 1, 256, 0)
        gpsdval = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(gpsdval, False , False, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("obd direct");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , False, 0)
        val = self.__getgconf( "OBDTHRU" )
        if not val:
            val = 22534
        adj = gtk.Adjustment(float(val), 1024, 65535, 1, 256, 0)
        obdt = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(obdt, False , False, 0)

        sep = gtk.HSeparator()
        hbox.pack_start(sep, True, False, 0)

        label = gtk.Label("gps direct");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , False, 0)
        val = self.__getgconf( "GPSTHRU" )
        if not val:
            val = 22947
        adj = gtk.Adjustment(float(val), 1024, 65535, 1, 256, 0)
        gpst = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(gpst, False , False, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("icon rate");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , False, 0)
        val = self.__getgconf( "ICONUPDATE" )
        if not val:
            val = 3
        adj = gtk.Adjustment(float(val), 1, 3600, 1, 60, 0)
        iconu = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(iconu, False , False, 0)

        label = gtk.Label("secs");
        label.set_width_chars(4);
        hbox.pack_start(label, False , False, 0)

        sep = gtk.HSeparator()
        hbox.pack_start(sep, True, False, 0)

        label = gtk.Label("Annote");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , False, 0)
        val = self.__getgconf( "GPSANNO" )
        if not val:
            val = 32947
        adj = gtk.Adjustment(float(val), 1024, 65535, 1, 256, 0)
        gpsa = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(gpsa, False , False, 0)

        #UNIXSOCK "/tmp/.gpsd_ctrl_sock"
        #ZIPKML "/usr/bin/kml2kmz"

        dialog.show_all()
        t = dialog.run()

        if t == gtk.RESPONSE_OK:
            val = httpval.get_value_as_int()
            self.__setgconf( "HTTPPORT", str(val) )
            val = gpsdval.get_value_as_int()
            self.__setgconf( "GPSDPORT", str(val) )
            val = obdt.get_value_as_int()
            self.__setgconf( "OBDTHRU", str(val) )
            val = gpst.get_value_as_int()
            self.__setgconf( "GPSTHRU", str(val) )
            val = gpsa.get_value_as_int()
            self.__setgconf( "GPSANNO", str(val) )

            val = iconu.get_value_as_int()
            self.__setgconf( "ICONUPDATE", str(val) )
            iconupdate = int(val)

            if hdenable.get_active():
                self.__setgconf( "USEHD", "1" );
            else:
                self.__setgconf( "USEHD", "0" );

            if obdenable.get_active():
                self.__setgconf( "USEOBD", "1" );
            else:
                self.__setgconf( "USEOBD", "0" );

            self.__setgconf( "OBDdev", obddev.get_text() )

            if vv1enable.get_active():
                self.__setgconf( "USEVV1", "1" );
            else:
                self.__setgconf( "USEVV1", "0" );

            self.__setgconf( "VV1dev", vv1dev.get_text() )

        dialog.destroy()


    def __gatedialog(self,widget):
        dialog = gtk.MessageDialog( None,
            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            gtk.MESSAGE_QUESTION, gtk.BUTTONS_OK_CANCEL, None)

        dialog.set_markup("Configure GPSGate")

        lwid = 8

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Enable");
        hbox.pack_start(label, False , True, 0)
        label.set_width_chars(lwid);
        t = self.__getgconf( "UseGPSGate" )
        internal = gtk.CheckButton("Enable");
        if t == "1":
            internal.set_active(1);
        hbox.pack_start(internal, True , True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Proto");
        hbox.pack_start(label, False , True, 0)
        label.set_width_chars(lwid);
        t = self.__getgconf( "GPSGATEtcp" )
        proto = gtk.CheckButton("Use TCP instead of UDP");
        if t == "1":
            proto.set_active(1);
        hbox.pack_start(proto, True , True, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Host");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)
        gpsgate = gtk.Entry()
        gpsgate.set_max_length(50)
        gpsgate.set_width_chars(36)
        t = self.__getgconf( "GPSGATEhost" )
        if not t:
            t = "online.gpsgate.com"
        gpsgate.set_text(t)
        hbox.pack_start(gpsgate, False , True, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Port");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)

        t = self.__getgconf( "GPSGATEport" )
        val = 30175
        if t:
            val = int(t)
        adj = gtk.Adjustment(float(val), 1, 65535, 1, 256, 0)
        ggport = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(ggport, False , False, 0)

        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("IMEI");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)
        imei = gtk.Entry()
        imei.set_max_length(50)
        imei.set_width_chars(20)
        t = self.__getgconf( "GPSGATEimei" )
        if not t:
            t = "(enter imei)"
        imei.set_text(t)
        hbox.pack_start(imei, False , True, 0)


        hbox = gtk.HBox(False, 0 )
        dialog.vbox.pack_start(hbox, True,True, 0)
        label = gtk.Label("Rate");
        label.set_width_chars(lwid);
        hbox.pack_start(label, False , True, 0)

        t = self.__getgconf( "GPSGATErate" )
        val = 5
        if t:
            val = int(t)
        adj = gtk.Adjustment(float(val), 1, 3600, 1, 60, 0)
        ggrate = gtk.SpinButton(adj, 0.0, 0)
        hbox.pack_start(ggrate, False , False, 0)

        dialog.show_all()
        t = dialog.run()
        if t == gtk.RESPONSE_OK:
            self.__setgconf( "GPSGATEhost", gpsgate.get_text() )
            val = ggport.get_value_as_int()
            self.__setgconf( "GPSGATEport", str(val) )
            val = ggrate.get_value_as_int()
            self.__setgconf( "GPSGATErate", str(val) )
            self.__setgconf( "GPSGATEimei", imei.get_text() )
            if internal.get_active():
                self.__setgconf( "UseGPSGate", "1" )
            else:
                self.__setgconf( "UseGPSGate", "0" )
            if proto.get_active():
                self.__setgconf( "GPSGATEtcp", "1" )
            else:
                self.__setgconf( "GPSGATEtcp", "0" )

        dialog.destroy()
    

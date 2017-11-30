#!/usr/bin/env python

import struct
import mmap
import time

import gtk
import pygtk
import gobject

try:
    import osso
except ImportError:
    osso = None

v1disp = [
"39 39 33 1",
"x c #00ff00",
"- c #ff00ff",
"r c #00ff00",
"r c #00ff00",
"r c #00ff00",
"s c #00ff00",
"s c #00ff00",
"s c #00ff00",
"u c #00ff00",
"u c #00ff00",
"u c #00ff00",
"l c #00ff00",
"q c #00ff00",
"k c #00ff00",
"f c #00ff00",
"g c #00ff00",
"8 c #00ff00",
"7 c #00ff00",
"6 c #00ff00",
"5 c #00ff00",
"4 c #00ff00",
"3 c #00ff00",
"2 c #00ff00",
"1 c #00ff00",
"p c #00ff00",
"a c #00ff00",
"b c #00ff00",
"c c #00ff00",
"d c #00ff00",
"e c #00ff00",
"m c #00ff00",
". c #000000",
"@ c #ffffff",

".......................................",
".......................................",
".......................................",
".......................................",
".......................................",
".......................................",
".......................................",
"...aaaaaaaaaa..@...lll.................",
"..f.aaaaaaaa.b.@...lll........uu.......",
"..ff........bb.@@@.lll.......uuuu......",
"..ff........bb..............uuuuuu.....",
"..ff........bb.............uuuuuuuu....",
"..ff........bb..@..qqq....uuuuuuuuuu...",
"..ff........bb.@@@.qqq...uuuuuuuuuuuu..",
"..ff........bb.@.@.qqq.....uuuuuuuu....",
"..ff........bb.............uuuuuuuu....",
"..ff........bb.............uuuuuuuu....",
"..fggggggggggb.@.@.kkk...s..........s..",
"..eggggggggggc.@@..kkk..ssssssssssssss.",
"..ee........cc.@.@.kkk.ssssssssssssssss",
"..ee........cc..........ssssssssssssss.",
"..ee........cc...........s..........s..",
"..ee........cc.@.@.xxx.....rrrrrrrr....",
"..ee........cc..@..xxx.....rrrrrrrr....",
"..ee........cc.@.@.xxx...rrrrrrrrrrrr..",
"..ee........cc.............rrrrrrrr....",
"..ee........cc..ppp..........rrrr......",
"..e.dddddddd.c..ppp....................",
"...dddddddddd...ppp....................",
".......................................",
"...888.777.666.555.444.333.222.111.....",
"...888.777.666.555.444.333.222.111.....",
"...888.777.666.555.444.333.222.111.....",
".......................................",
"...mmmmm..m...m.mmmmm..mmmmm...........",
"...m.m.m..m...m...m....m...............",
"...m.m.m..m...m...m....mmmmm...........",
"...m.m.m..m...m...m....m...............",
"...m.m.m...mmm....m....mmmmm...........",
               ]
v1prev = 0
v1blink = 0
button = None
disp = None
osso_app = None

def dosegment(seg):
    seg += 1;
    if (v1blink >> seg) & 1:
        v1disp[seg] = v1disp[seg][0:5] + "ffff00"
    elif (v1prev >> seg) & 1:
        v1disp[seg] = v1disp[seg][0:5] + "ff0000"
    else:
        v1disp[seg] = v1disp[seg][0:5] + "300000"

def updatev1():
    global v1prev, v1blink, button, disp
    gobject.timeout_add(25,updatev1)
    v1state = struct.unpack("2i",data)

    v1dstate = v1state[0] ^ v1prev
    v1dblink = v1state[1] ^ v1blink

    if v1dstate == 0: # and v1dblink == 0 :
        return;

    if disp:
        disp.display_state_on()

    window.present()

    v1prev = v1state[0]
    v1blink = v1state[1]

    for i in range (31):
        if v1dstate >> (1+i) & 1 or v1dblink >> (1+i):
            dosegment(i)

    x = v1prev >> 1 & 1
    r = v1prev >> 5 & 1
    s = v1prev >> 8 & 1
    u = v1prev >> 11 & 1

    l = v1prev >> 12 & 1
    ka = v1prev >> 13 & 1
    k = v1prev >> 14 & 1

    p = v1prev >> 25 & 1
    m = v1prev >> 31 & 1
    
    sevseg = v1prev >> 24 & 0x7c
    sevseg |= v1prev >> 15 & 3;

    stren = v1prev >> 17 & 0xff

#    if stren == 0 && timeout
#        window.iconify()

    decode=[]
    for i in range( 256 ):
        decode += "+";
    """
         4
     1       8
         2
    64      16
        32
    """        
    decode[0] = ' '
    decode[24] = '1'
    decode[27] = '4'
    decode[28] = '7'
    decode[32] = '_'#
    decode[38] = '#'##
    decode[55] = '5'
    decode[60] = ']'#
    decode[62] = '3'
    decode[63] = '9'
    decode[65] = '|'#
    decode[71] = 'F'
    decode[79] = 'P'
    decode[91] = 'H'
    decode[95] = 'A'
    decode[96] = 'l'
    decode[97] = 'L'
    decode[101] = 'C'
    decode[103] = 'E'
    decode[110] = '2'
    decode[112] = 'u'
    decode[114] = 'o'
    decode[115] = 'b'
    decode[116] = 'v'##
    decode[117] = 'G'
    decode[119] = '6'
    decode[120] = 'J'
    decode[121] = 'U'#
    decode[122] = 'd'
    decode[125] = '0'#
    decode[127] = '8'

    if( stren == 254 ) :
        stren = 9
    else :
        i = 0
        while stren :
            stren >>= 1
            i += 1
        stren = i
        
    
    print "%8x %8x"%(v1state), " ^"[u]+" -"[s]+" v"[r] , " X"[x]+" K"[k]+" A"[ka]+" L"[l] , decode[sevseg]+" ."[p], stren, m,  sevseg

    pixbuf = gtk.gdk.pixbuf_new_from_xpm_data(v1disp)
    image = gtk.Image()
    w = button.get_allocation().width
    h = button.get_allocation().height
    if w > h:
        w = h
    image.set_from_pixbuf(pixbuf.scale_simple(w,w,gtk.gdk.INTERP_NEAREST))
    button.set_image(image)

try:
    file = open("/tmp/v1state", "r+")
    data = mmap.mmap(file.fileno(), 8)
except (ValueError, IOError):
    exit()

for i in range (31):
    dosegment(i)

if osso:
    osso_c = osso.Context("v1disp", "0.0.1", False)
    disp = osso.DeviceState(osso_c)
    osso_app = osso.Application(osso_c)

window = gtk.Window(gtk.WINDOW_TOPLEVEL)
window.connect("destroy", lambda wid: gtk.main_quit())
window.connect("delete_event", lambda a1,a2:gtk.main_quit())
window.set_title("Valentine V1")

button = gtk.Button()
button.set_image(gtk.image_new_from_stock(gtk.STOCK_YES, gtk.ICON_SIZE_MENU))
button.set_name("menu")
button.set_size_request(320,320)
button.show()

window.add(button)
window.show()

gobject.timeout_add(25,updatev1)
gtk.main()

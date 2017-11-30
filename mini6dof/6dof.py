#!/usr/bin/env python

import pygtk
import gtk
import gobject
import operator
import time
import string
import math
import socket
import select

import hildon
import struct
import mmap
import time

class DashBoard:

	boxiz = 696
	height = 396
	width = 696
	mrgn = boxiz/32
	crad = boxiz/2-mrgn*2

	def __init__(self):
		self.app = hildon.Program()
		self.window = hildon.Window()
		self.app.add_window(self.window)

		self.window.set_title("Dashboard")
		self.window.connect("destroy", gtk.main_quit)
		self.window.connect("window-state-event", self.on_window_state_change)
		self.window.connect("key-press-event", self.on_key_press)
		self.window_in_fullscreen = False
		self.area = gtk.DrawingArea()
		self.area.set_size_request(696,396)
		self.window.add(self.area)
		self.area.connect("expose-event", self.area_draw_cb)
		self.area.connect("configure-event", self.on_configure)
		self.area.show()
		self.window.show()
		self.style = self.area.get_style()
		self.gc = self.style.fg_gc[gtk.STATE_NORMAL]
		self.draw_base()
#try/except IOError, etc.
		self.sdoffile = open("/tmp/sdofstate", "r+")
		self.sdofdata = mmap.mmap(self.sdoffile.fileno(), 52)
                self.sdofmem = ()
                for i in range(13):
                        self.sdofmem += (-1515870811,)
		gobject.timeout_add(100,self.sockcheck)

	def on_configure(self, widget, event ):
		x,y,self.width,self.height = widget.get_allocation()
		self.boxiz = self.width/2
		if self.boxiz > self.height:
			self.boxiz = self.height

	def on_window_state_change(self, widget, event, *args):
		if event.new_window_state & gtk.gdk.WINDOW_STATE_FULLSCREEN:
			self.window_in_fullscreen = True
		else:
			self.window_in_fullscreen = False
		self.draw_base()
	
	def on_key_press(self, widget, event, *args):
		if event.keyval == gtk.keysyms.F6:
			if self.window_in_fullscreen:
				self.window.unfullscreen()
			else:
				self.window.fullscreen()

	def area_draw_cb( self, area, event):
		self.style = self.area.get_style()
		self.gc = self.style.fg_gc[gtk.STATE_NORMAL]
		self.draw_base()
		return True

########################################################################
	count = 0
        
	def sockcheck(self):
                sdofstate = struct.unpack("13i",self.sdofdata)
                if sdofstate != self.sdofmem :
                        #print sdofstate
                        self.sdofmem = sdofstate
                # pitch x x roll x x yaw x x X Y Z x
                self.drawxyz(sdofstate[9],sdofstate[10],sdofstate[11]);
                self.drawpry(sdofstate[0],sdofstate[3],sdofstate[6]);
                
		gobject.timeout_add(50,self.sockcheck)

        def drawxyz(self,x,y,z):
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0) )
		self.area.window.draw_rectangle(self.gc, True, 0, 0, self.width/2, self.height)
                ave = 350
                x -= ave
                y -= ave
                z -= ave
                z /= 3
                x /= 2
                y /= 2
                x += self.width/4
                y += self.height/2
                z += self.boxiz/4
                x -= z/2
                y -= z/2
                w = h = z
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535) )
                self.area.window.draw_arc(self.gc, False, x,y,w,h, 0, 360*64);
                                
        def drawpry(self,x,y,z):
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0) )
                self.area.window.draw_rectangle(self.gc, True, self.width/2, 0, self.width/2, self.height)
                ave = 512
                x -= ave
                y -= ave
                z -= ave
                x /= 2
                y /= 2
                z /= 4
                x += self.width*3/4
                y += self.height/2
                z += self.boxiz/2
                x -= z/2
                y -= z/2
                w = h = z
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535) )
                self.area.window.draw_arc(self.gc, False, x,y,w,h, 0, 360*64);

########################################################################

	sinetab = eval( """[ 
	    0, 571, 1143, 1714, 2285, 2855, 3425, 3993, 4560, 5126, 5690, 6252, 6812,
	    7371, 7927, 8480, 9032, 9580, 10125, 10668, 11207, 11743, 12275, 12803,
	    13327, 13848, 14364, 14876, 15383, 15886, 16383, 16876, 17364, 17846, 18323,
	    18794, 19260, 19720, 20173, 20621, 21062, 21497, 21926, 22347, 22762, 23170,
	    23571, 23964, 24351, 24730, 25101, 25465, 25821, 26169, 26509, 26841, 27165,
	    27481, 27788, 28087, 28377, 28659, 28932, 29196, 29451, 29697, 29935, 30163,
	    30381, 30591, 30791, 30982, 31164, 31336, 31498, 31651, 31794, 31928, 32051,
	    32165, 32270, 32364, 32449, 32523, 32588, 32643, 32688, 32723, 32748, 32763,
	    32768 ]""")

	def isin(self, angle):
		while angle >= 360:
			angle -= 360
		while angle < 0:
			angle += 360
		if angle <= 90:
			return self.sinetab[angle]
		elif angle <= 180:
			return self.sinetab[180 - angle]
		elif angle <= 270:
			return -self.sinetab[angle - 180]
		else:
			return -self.sinetab[360 - angle]

	def icos(self, angle):
		return self.isin(angle + 90)

########################################################################

	def draw_base(self):
		self.gc.set_function(gtk.gdk.COPY)
		self.gc.set_line_attributes(3,gtk.gdk.LINE_SOLID, gtk.gdk.CAP_ROUND, gtk.gdk.JOIN_ROUND)

		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0) )
                self.gc.set_rgb_bg_color( gtk.gdk.Color(0,0,0) )

		self.area.window.draw_rectangle(self.gc, True, 0, 0, self.width, self.height)
#		self.gc.set_rgb_fg_color( gtk.gdk.Color(24576,24576,24576,0))
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535))
		temp = self.boxiz/4
		self.gc.set_line_attributes(8,gtk.gdk.LINE_SOLID, gtk.gdk.CAP_ROUND, gtk.gdk.JOIN_ROUND)
                self.obdmem = ()


def main():
	    gtk.main()
	    return 0
	
if __name__ == "__main__":
	    DashBoard()
	    main()


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

import dbus, dbus.service
from dbus.mainloop.glib import DBusGMainLoop

class DashBoard:
	height = 396
	width = 696
        trk=rpm=sgnl=gear=hot=gas=gear=spd = -1
        
	def __init__(self):

		self.app = hildon.Program()
		self.window = hildon.Window()
		self.app.add_window(self.window)

		self.window.set_title("Harley Instrument Cluster")
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

                try:
                        self.file = open("/tmp/mgpstate", "r+")
                        self.data = mmap.mmap(self.file.fileno(), 300)
                except (ValueError, IOError):
                        #exit()
                        self.data = ""
                        for t in range(300):
                                self.data += chr(0)

                self.active = 1
                self.m_loop = DBusGMainLoop()
                self.bus = dbus.SystemBus(mainloop = self.m_loop, private = True)
                self.handler = self.bus.add_signal_receiver(self._inactivity_handler,
                                                            signal_name="display_status_ind",
                                                            dbus_interface='com.nokia.mce.signal',
                                                            path='/com/nokia/mce/signal')

		gobject.timeout_add(100,self.updater)

        def _inactivity_handler(self, inact=None):
                if inact == "off":
                        self.active = 0
                else:
                        if self.active == 0:
                                gobject.timeout_add(100,self.updater)
                        self.active = 1

	def on_configure(self, widget, event ):
		x,y,self.width,self.height = widget.get_allocation()
                self.trk=self.rpm=self.sgnl=self.gear=self.hot=self.gas=self.gear=self.spd = -1;

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

        trk=rpm=sgnl=gear=hot=gas=gear=spd = -1;
        count=0
        odolast=fullast=0
	def updater(self):
		gpstate = struct.unpack("21i80h14i",self.data)
# rpm, vspd, full, gear, clutch, neutral, engtemp, turnsig, odoaccum, fuelaccum; odolastms, fuellastms; odolastval, fuellastval;
		t = gpstate[101] / 1000
                if t != self.rpm :
                        self.rpm = t
                        self.draw_numbers( self.width*3/8 , 0 , 8, int(t), 4)

                if self.rpm > 0 :                        
                        t = gpstate[102] / 1000
                else:
                        t = gpstate[6] / 1000
                if t != self.spd :
                        self.spd = t
                        self.draw_numbers( -self.width/8 , 0 , 8, int(t), 3)

		t = gpstate[7] / 1000
                if t != self.trk :
                        self.trk = t
                        if self.spd > 0 :
                                self.draw_compass(360-self.trk)
                        else :
                                self.draw_compass(-1)

                t = gpstate[103]                        
                if t != self.gas :
                        self.gas = t
                        self.draw_numbers( self.width*1/8 , self.height/2 , 4, int(t), 1)

		t = gpstate[107]
                if t != self.hot :
                        self.hot = t
                        self.draw_numbers( 0, self.height*3/4 , 4, int(t), 3)

		t = gpstate[108]
                if t != self.sgnl :
                        self.sgnl = t
			self.draw_signal(t)

                self.count += 1
                if self.count > 49 :
                        self.count = 0
                        odo = gpstate[109] - self.odolast
                        ful = gpstate[110] - self.fullast
                        self.odolast = gpstate[109]
                        self.fullast = gpstate[110]
                        if ful == 0 :
                                ful = 1
                        
                        t = odo * 20 / ful;
                        if t > 99 :
                                t = 99
                        self.draw_numbers( self.width*13/16, self.height*3/4 , 4, int(t), 2)
                       
                # ful * 40 / fultime # gives gallons per hour
                #hpg = 100 * fultime / ful

                t = (gpstate[104]&7) | gpstate[105]*8 | gpstate[106]*16                
                if t != self.gear :
                        self.gear = t
                        gear = gpstate[104] & 7
                        clu = gpstate[105]
                        neutral = gpstate[106]

                        if neutral != 0 :
                                self.draw_clu(0)
                        elif clu != 0 :
                                self.draw_clu(12)
                        elif gear < 1 or gear > 6 :
                                self.draw_clu(16)
                        else :
                                self.draw_clu(17)
                                self.draw_gear(gear)

                if self.active:
                        gobject.timeout_add(100,self.updater)

	def draw_gear(self,gear):
        	self.draw_number( self.width*6/8 , self.height*3/4, 8, gear)

	def draw_clu(self,cln):
                self.draw_number( self.width*5/8 , self.height*3/4, 8, cln)

########################################################################
########################################################################

	def draw_base(self):
		self.gc.set_function(gtk.gdk.COPY)
		self.gc.set_line_attributes(3,gtk.gdk.LINE_SOLID, gtk.gdk.CAP_ROUND, gtk.gdk.JOIN_ROUND)
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0) )
		self.area.window.draw_rectangle(self.gc, True, 0, 0, self.width, self.height)
                self.trk=self.rpm=self.sgnl=self.gear=self.hot=self.gas=self.gear=self.spd = -1;


	sevsegs = [ # C T B UL LL UR LR L R
		(-4,0,4,0),
		(-3,-8,5,-8),
		(-5,8,3,8),
		(-3,-8,-4,-1),
		(-4,1,-5,8),
		(5,-8,4,-1),
		(4,1,3,8),
		(-3,-8,-5,8), #7=3,4
		(5,-8,3,8), #8=5,6
		]

	nums = [
		(1,2,7,8),
		(8,),
		(0,1,2,4,5),
		(0,1,2,8),
		(0,3,8),
		(0,1,2,3,6),
		(0,1,2,6,7),
		(1,8),
		(0,1,2,7,8),
		(0,1,2,3,8),
                (0,1,7,8),
		(0,2,6,7),
                (1,2,7),
                (0,2,4,8),
                (0,1,2,7),
                (0,1,7),
                (0,),
                (),
		]

	unnums = [
		(0,),
		(0,1,2,7),
		(3,6),
		(7,),
		(1,2,4),
		(4,5),
		(5,),
		(0,2,7),
		(),
		(4,),
                (2,),
                (1,5),
                (0,8),
                (1,3),
                (8,),
                (2,8),
                (1,2,7,8),
                (0,1,2,7,8),
		]


	def draw_number(self,x,y,scale,val):

		self.gc.set_line_attributes(scale*2+2,gtk.gdk.LINE_SOLID,  gtk.gdk.CAP_PROJECTING, gtk.gdk.JOIN_MITER)
		segs = []
		for j in self.unnums[val]:
			i = self.sevsegs[j]
			segs += [( x+scale*i[0],y+scale*i[1],x+scale*i[2],y+scale*i[3] )]
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0))
		self.area.window.draw_segments(self.gc, segs)

		self.gc.set_line_attributes(scale*2,gtk.gdk.LINE_SOLID,  gtk.gdk.CAP_PROJECTING, gtk.gdk.JOIN_MITER)
		segs = []
		for j in self.nums[val]:
			i = self.sevsegs[j]
			segs += [( x+scale*i[0],y+scale*i[1],x+scale*i[2],y+scale*i[3] )]
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535))
		self.area.window.draw_segments(self.gc, segs)


	def draw_numbers(self,x,y,scale,val,wid):
                nums = []
                if val < 0:
                        nums.append( 16 )
                        val = -val
                nums.append( val % 10 )
		while val > 9 :
			val /= 10
			nums.append( val % 10 )
                while len(nums) < wid:
                        nums.append( 17 )
                nums.reverse()
                x += 12 * scale * wid
                y += 10 * scale
		while len(nums):
			self.draw_number( x,y , scale, nums.pop())
			x -= 12 * scale


########################################################################
	def draw_fuelspin(self,val):
		temp = self.width/8
                temph = self.height/4
                if temp > temph:
                        temp = temph
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0))
                self.area.window.draw_arc(self.gc, True, self.width/8, self.height/2, temp, temp,90*64-val,90*64)
                self.area.window.draw_arc(self.gc, True, self.width/8, self.height/2, temp, temp,270*64-val,90*64)
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535))
                self.area.window.draw_arc(self.gc, True, self.width/8, self.height/2, temp, temp,-val,90*64)
                self.area.window.draw_arc(self.gc, True, self.width/8, self.height/2, temp, temp,180*64-val,90*64)
        	
	def draw_signal(self,sgnl):
		self.gc.set_function(gtk.gdk.COPY)
		temp = self.width/8
                th = self.height /2
		arrow = [ (0,th+temp/2),(temp,th),(temp,th+temp) ]
		if sgnl & 2 == 0 :
			self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0))
		else:
			self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535))
	        self.area.window.draw_polygon(self.gc, True, arrow)

		arrow = [ (self.width,th+temp/2),(self.width-temp,th),(self.width-temp,th+temp) ]
		if sgnl & 1 == 0 :
			self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0))
		else:
			self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535))
		self.gc.set_line_attributes(1,gtk.gdk.LINE_SOLID,  gtk.gdk.CAP_PROJECTING, gtk.gdk.JOIN_MITER)
	        self.area.window.draw_polygon(self.gc, True, arrow)

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



	def aetoxy( self, size, azim, dist ):
		tlx = size*3/2 + dist * ( self.isin( azim ) * size / 32768 ) / 90
		tly = size*3/2 - dist * ( self.isin( 90-azim ) * size / 32768 ) / 90
		return (tlx,tly)

	def draw_compass(self,dir):
		temp = self.width/8
                temph = self.height/4
                if temp > temph:
                        temp = temph
                temp = temp * 10 / 9
                self.gc.set_function(gtk.gdk.COPY)
                self.gc.set_rgb_fg_color( gtk.gdk.Color(16384,8192,0))
                self.area.window.draw_arc(self.gc, True, temp*2, temp*2, temp*2, temp*2,0,360*64)
                if dir < 0 :
                        return
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535))
		t1 = self.aetoxy( temp*2, dir, temp*5/20 )
		t2 = self.aetoxy( temp*2, 90 + dir, temp/20 )
		t3 = self.aetoxy( temp*2, 180 + dir, temp/10 )
		t4 = self.aetoxy( temp*2, 270 + dir, temp/20 )
		segsc = [t1,t2,t3,t4]
		self.gc.set_line_attributes(12,gtk.gdk.LINE_SOLID,  gtk.gdk.CAP_PROJECTING, gtk.gdk.JOIN_MITER)
	        self.area.window.draw_polygon(self.gc, False, segsc )




def main():
	    gtk.main()
	    return 0
	
if __name__ == "__main__":
	    DashBoard()
	    main()

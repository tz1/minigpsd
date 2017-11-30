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

                try:
                        self.gpsfile = open("/tmp/mgpstate", "r+")
                        self.gpsdata = mmap.mmap(self.gpsfile.fileno(), 300)
                except (ValueError, IOError):
                        #exit()
                        self.gpsdata = ""
                        for t in range(300):
                                self.gpsdata += chr(0)
                        
                self.obdmem = ()
                for i in range(128):
                        self.obdmem += (-1515870811,)

                try:
                        self.obdfile = open("/tmp/obd2state", "r+")
                        self.obddata = mmap.mmap(self.obdfile.fileno(), 512)
                except (ValueError, IOError):
                        #exit()
                        self.obddata = ""
                        for t in range(512):
                                self.obddata += chr(0)

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
#		gpstate = struct.unpack("21i80h",self.gpsdata)
                gpstate = struct.unpack("21i80h14i",self.gpsdata)
                obdstate = struct.unpack("128i",self.obddata)
    
		if not True:
			print "lat=",float(gpstate[0])/1000000
			print "lon=",float(gpstate[1])/1000000
			print "alt=",float(gpstate[2])/1000
			print "pdop=",float(gpstate[3])/1000
			print "hdop=",float(gpstate[4])/1000
			print "vdop=",float(gpstate[5])/1000
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

		trk = gpstate[7] / 1000		
		spd = gpstate[6] / 1000
                
		rpm = obdstate[12]
                if rpm != self.obdmem[12]:
			self.draw_dial2(rpm/4)
                
		spd = obdstate[13]
                if spd != self.obdmem[13]:
			self.draw_dial1(spd)

                hot = obdstate[5]
                if hot != self.obdmem[5]:
			self.draw_status(2,hot-40)

                self.obdmem = obdstate

                #should add minimum speed, maybe withing compass
		if trk >= 0 :
			self.draw_compass(360-trk)

		gobject.timeout_add(150,self.sockcheck)

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

	segs1 = [ (-100,-100,-100,-100) ]
	segs2 = [ (-100,-100,-100,-100) ]
	segsc = [ (-100,-100), (-100,-100), (-100,-100) ]
	def draw_base(self):
		self.gc.set_function(gtk.gdk.COPY)
		self.gc.set_line_attributes(3,gtk.gdk.LINE_SOLID, gtk.gdk.CAP_ROUND, gtk.gdk.JOIN_ROUND)
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0) )

		self.area.window.draw_rectangle(self.gc, True, 0, 0, self.width, self.height)
#		self.gc.set_rgb_fg_color( gtk.gdk.Color(24576,24576,24576,0))
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,0,0,0))
		temp = self.boxiz/4
		self.area.window.draw_arc(self.gc, False, 0,0, temp*4, temp*4, 210*64, -240*64)
		self.area.window.draw_arc(self.gc, False, temp*4,0, temp*4, temp*4, 210*64, -240*64)
		self.area.window.draw_arc(self.gc, False, temp,temp, temp*2, temp*2, 210*64, -240*64)
		self.area.window.draw_arc(self.gc, False, temp*5,temp, temp*2, temp*2, 210*64, -240*64)

		segments = []
		for angle in range( 210, -31, -20 ):
			xd = self.icos(angle) * temp / 32768
			yd = self.isin(angle) * temp / 32768
			segments += [
			    (temp*2+xd,temp*2-yd,temp*2+xd*2,temp*2-yd*2),
			]
		self.area.window.draw_segments(self.gc, segments)

		segments = []
		for angle in range( 210, -40, -30 ):
			xd = self.icos(angle) * temp / 32768
			yd = self.isin(angle) * temp / 32768
			segments += [
			    (temp*6+xd,temp*2-yd,temp*6+xd*2,temp*2-yd*2),
		    	]
		self.area.window.draw_segments(self.gc, segments)

		self.segs1 = [ (-100,-100,-100,-100) ]
		self.segs2 = [ (-100,-100,-100,-100) ]
		self.segsc = [ (-100,-100), (-100,-100), (-100,-100) ]
		self.oldval2 = ""
		self.gc.set_line_attributes(8,gtk.gdk.LINE_SOLID, gtk.gdk.CAP_ROUND, gtk.gdk.JOIN_ROUND)
                self.obdmem = ()
                for i in range(128):
                        self.obdmem += (-1515870811,)

#technically draw_segments should be draw_line
	def draw_dial1(self,val):
		self.gc.set_function(gtk.gdk.XOR)# or invert
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535,0))
		self.area.window.draw_segments(self.gc, self.segs1)
		temp = self.boxiz/4
		angle = 210 - val * 2; #.5mph per degree
		xd = self.icos(angle) * temp / 32768
		yd = self.isin(angle) * temp / 32768
		self.segs1 = [(temp*2+xd,temp*2-yd,temp*2+xd*2,temp*2-yd*2)]
		self.area.window.draw_segments(self.gc, self.segs1)

	def draw_dial2(self,val):
		self.gc.set_function(gtk.gdk.XOR)# or invert
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535,0))
		self.area.window.draw_segments(self.gc, self.segs2)
		temp = self.boxiz/4
		angle = 210 - val * 3 / 100; # 240 deg / 8000 rpm = 3 / 100
		xd = self.icos(angle) * temp / 32768
		yd = self.isin(angle) * temp / 32768
		self.segs2 = [(temp*6+xd,temp*2-yd,temp*6+xd*2,temp*2-yd*2)]
		self.area.window.draw_segments(self.gc, self.segs2)


	chars =[
[ -1,-11, -4,-10, -6,-7, -7,-2, -7,1, -6,6, -4,9, -1,10, 1,10, 4,9, 6,6, 7,1, 7,-2, 6,-7, 4,-10, 1,-11, -1,-11, ],
[ -4,-7, -2,-8, 1,-11, 1,10, ],
[ -6,-6, -6,-7, -5,-9, -4,-10, -2,-11, 2,-11, 4,-10, 5,-9, 6,-7, 6,-5, 5,-3, 3,0, -7,10, 7,10, ],
[ -5,-11, 6,-11, 0,-3, 3,-3, 5,-2, 6,-1, 7,2, 7,4, 6,7, 4,9, 1,10, -2,10, -5,9, -6,8, -7,6, ],
[ 3,10, 3,-11, -7,3, 8,3, ],
[ 5,-11, -5,-11, -6,-2, -5,-3, -2,-4, 1,-4, 4,-3, 6,-1, 7,2, 7,4, 6,7, 4,9, 1,10, -2,10, -5,9, -6,8, -7,6, ],
[ 6,-8, 5,-10, 2,-11, 0,-11, -3,-10, -5,-7, -6,-2, -6,3, -5,7, -3,9, 0,10, 1,10, 4,9, 6,7, 7,4, 7,3, 6,0, 4,-2, 1,-3, 0,-3, -3,-2, -5,0, -6,3, ],
[ -7,-11, 7,-11, -3,10, ],
[ -2,-11, -5,-10, -6,-8, -6,-6, -5,-4, -3,-3, 1,-2, 4,-1, 6,1, 7,3, 7,6, 6,8, 5,9, 2,10, -2,10, -5,9, -6,8, -7,6, -7,3, -6,1, -4,-1, -1,-2, 3,-3, 5,-4, 6,-6, 6,-8, 5,-10, 2,-11, -2,-11, ],
[ 6,-4, 5,-1, 3,1, 0,2, -1,2, -4,1, -6,-1, -7,-4, -7,-5, -6,-8, -4,-10, -1,-11, 0,-11, 3,-10, 5,-8, 6,-4, 6,1, 5,6, 3,9, 0,10, -2,10, -5,9, -6,7, ],
		]

	def xdraw_number(self,x,y,scale,val):
		segs = []
		cseg = self.chars[val]
		for i in range(0, len(cseg),2):
			segs += [ (x+scale*cseg[i],y+scale*cseg[i+1]) ]
		self.area.window.draw_lines(self.gc, segs)


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
		(0,1,2,3,4,6),
		(1,8),
		(0,1,2,7,8),
		(0,1,2,3,8),
		]

	def draw_number(self,x,y,scale,val):
		segs = []
		for j in self.nums[val]:
			i = self.sevsegs[j]
			segs += [( x+scale*i[0],y+scale*i[1],x+scale*i[2],y+scale*i[3] )]
		self.area.window.draw_segments(self.gc, segs)


	def draw_numbers(self,x,y,scale,val):
		#fixme - handle negative
		nums = [ val % 10 ]
		while val > 9 :
			val /= 10
			nums.append( val % 10 )

		while len(nums):
			self.draw_number( x,y , scale, nums.pop())
			x += 20 * scale

	def draw_gear(self,val):
		temp = self.boxiz/4
		self.gc.set_function(gtk.gdk.COPY)
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0))
		self.area.window.draw_rectangle(self.gc, True, temp*6-45, temp*2-60, 90,120 )
		if( val < 0 or val > 6 ):
			return
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535,0))
        	self.draw_number( temp*6 , temp*2 , 5, val)


	def draw_status(self,place, val):
		temp = self.boxiz/4
		self.gc.set_function(gtk.gdk.COPY)
		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0))

		y = 3*temp
		x = place*2*temp

		self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0))
		self.area.window.draw_rectangle(self.gc, True, x,y, temp*2 ,temp)
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535,0))
        	self.draw_numbers( x+temp/2 , y+temp/2 , 2, int(val))


	def aetoxy( self, size, azim, dist ):
		tlx = size + dist * ( self.isin( azim ) * size / 32768 ) / 90
		tly = size - dist * ( self.isin( 90-azim ) * size / 32768 ) / 90
		return (tlx,tly)

	def draw_compass(self,dir):
		temp = self.boxiz/4
		if dir < 0 :
			self.gc.set_function(gtk.gdk.COPY)
			self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0))
			self.area.window.draw_arc(self.gc, True, temp, temp, temp*2, temp*2,0,360*64)
			return
		self.gc.set_function(gtk.gdk.XOR)
		self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535,0))
	        self.area.window.draw_polygon(self.gc, False, self.segsc)

		t1 = self.aetoxy( temp*2, dir, temp/2 - 8 )
		t2 = self.aetoxy( temp*2, 90 + dir, temp/20 )
		t3 = self.aetoxy( temp*2, 180 + dir, temp/10 )
		t4 = self.aetoxy( temp*2, 270 + dir, temp/20 )
		self.segsc = [t1,t2,t3,t4]
	        self.area.window.draw_polygon(self.gc, False, self.segsc )

	def draw_signal(self,sgnl):
		if sgnl < 0 or sgnl > 3 : 
			return

		self.gc.set_function(gtk.gdk.COPY)

		temp = self.boxiz/8
		arrow = [ (0,temp/2),(temp,0),(temp,temp) ]
		if sgnl & 1 == 0 :
			self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0))
		else:
			self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535,0))
	        self.area.window.draw_polygon(self.gc, True, arrow)

		arrow = [ (self.boxiz*2,temp/2),(self.boxiz*2-temp,0),(self.boxiz*2-temp,temp) ]
		if sgnl & 2 == 0 :
			self.gc.set_rgb_fg_color( gtk.gdk.Color(0,0,0,0))
		else:
			self.gc.set_rgb_fg_color( gtk.gdk.Color(65535,65535,65535,0))
	        self.area.window.draw_polygon(self.gc, True, arrow)




def main():
	    gtk.main()
	    return 0
	
if __name__ == "__main__":
	    DashBoard()
	    main()


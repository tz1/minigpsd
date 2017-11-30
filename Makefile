CFLAGS=-O6 -Wall -I./minigpsd/ -U_FORTIFY_SOURCE

.PHONY: gpsd obd2 v1d gpsplayback

#cross
all: gpsd obd2 v1d gpsplayback

install-python:
	install -d $(DESTDIR)/usr/bin
	install setup-minigpsd $(DESTDIR)/usr/bin
	install miniobd2d/obd2dash.py $(DESTDIR)/usr/bin
	install minigpsd/harleydash.py $(DESTDIR)/usr/bin
	install minigpsd/v1state.py $(DESTDIR)/usr/bin
	install minigpsd/3dk.py $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib/hildon-desktop
	install minigpsd-sb.py $(DESTDIR)/usr/lib/hildon-desktop
	install minigpsd-tb.py $(DESTDIR)/usr/lib/hildon-desktop
	install -d $(DESTDIR)/usr/lib/python2.5
	install mgsetuplib.py $(DESTDIR)/usr/lib/python2.5
	install -d $(DESTDIR)/usr/share/applications/hildon
	install setup-minigpsd.desktop miniobd2d/obd2dash.desktop minigpsd/harleydash.desktop $(DESTDIR)/usr/share/applications/hildon
	install -d $(DESTDIR)/usr/share/applications/hildon-status-bar
	install minigpsd-sb.desktop $(DESTDIR)/usr/share/applications/hildon-status-bar
	install -d $(DESTDIR)/usr/share/applications/hildon-navigator
	install minigpsd-tb.desktop $(DESTDIR)/usr/share/applications/hildon-navigator

install: install-python
	install -d $(DESTDIR)/usr/bin
	install -s miniobd2d/miniobd2d miniv1d/miniv1d miniobd2d/obdecode minigpsd/minigpsd minigpsd/kml2kmz $(DESTDIR)/usr/bin
	install -s gpsplayback/gpsplayback $(DESTDIR)/usr/bin
	install btconnect.sh $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/etc/sudoers.d
	install l2ping.sudoers $(DESTDIR)/etc/sudoers.d
#	install n810asbtgps.sh $(DESTDIR)/usr/bin
#	install ossonotify $(DESTDIR)/usr/bin

#now done with docs
#	install -d $(DESTDIR)/etc/osso-backup/applications
#	install minigpsd.back.conf $(DESTDIR)/etc/osso-backup/applications

gpsplayback:
	make -C gpsplayback

gpsd:
	make -C minigpsd

obd2:
	make -C miniobd2d

v1d:
	make -C miniv1d

clean:
	make -C minigpsd clean
	make -C miniobd2d clean
	make -C miniv1d clean
	make -C gpsplayback clean

#bdb: bdb.c
#	gcc $(CFLAGS) -s -o bdb bdb.c -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include -ldbus-1

icon:
	echo "XB-Maemo-Icon-26:" >minigpsd.maemoicon
	openssl base64 <minigpsd.png | sed -e "s/^/ /g" >>minigpsd.maemoicon

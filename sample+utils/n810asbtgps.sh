#!/bin/sh
if [ $# == 0 ]; then
        sdptool add SP
        while true; do
                rfcomm -r -i hci0 listen 2 1 $0 {}
        done
fi
exec /usr/libexec/navicore-gpsd-helper >$1

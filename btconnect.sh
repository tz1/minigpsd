#!/bin/sh  

until sudo /usr/bin/l2ping -t 1 -c 1 $2 >/dev/null 2>/dev/null; do  
    sleep 3
done  
exec rfcomm connect $1 $2 



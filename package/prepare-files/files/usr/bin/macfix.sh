#!/bin/sh

if [ "$#" -ne 1 ];then
	echo "Not macaddress specified"
	exit 0
fi

dd if=/dev/mtd2 of=/tmp/factory_bak 2>/dev/null

mac_5g="$1"
real_mac_5g="\x${mac_5g:0:2}\x${mac_5g:3:2}\x${mac_5g:6:2}\x${mac_5g:9:2}\x${mac_5g:12:2}\x${mac_5g:15:2}"
printf "$real_mac_5g" |dd of=/tmp/factory_bak bs=1 count=6 seek=$((0x8004)) conv=notrunc 2>/dev/null

mtd write /tmp/factory_bak factory

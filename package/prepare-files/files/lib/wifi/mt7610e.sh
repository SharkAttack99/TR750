#!/bin/sh
append DRIVERS "mt7610e"

. /lib/wifi/ralink_common.sh

prepare_mt7610e() {
	prepare_ralink_wifi mt7610e
}

scan_mt7610e() {
	scan_ralink_wifi mt7610e mt7610e
}

disable_mt7610e() {
	disable_ralink_wifi mt7610e
}

enable_mt7610e() {
	enable_ralink_wifi mt7610e mt7610e
}

detect_mt7610e() {
	local device=`cat /proc/net/dev | grep rai0`
	if [ -z "$device" ];then
		return
	fi

#	detect_ralink_wifi mt7610e mt7610e
	ssid=Meizu-R13-5G-`eth_mac r wl1 | cut -c 12- | sed 's/://g'`
	cd /sys/module
	[ -d $module ] || return
        [ -e /etc/config/wireless ] && return
         cat <<EOF
config wifi-device      mt7610e
        option type     mt7610e
        option vendor   ralink
        option band     5G
        option channel  0
        option auotch   2
        option bw       2
        option ht_gi    1
        option country  CN
        option aregion  4

config wifi-iface	mt7610eiface
        option device   mt7610e
        option ifname   rai0
        option network  lan
        option mode     ap
        option ssid     $ssid
        option encryption none

EOF
}

#!/bin/sh
append DRIVERS "mt7628"

. /lib/wifi/ralink_common.sh

prepare_mt7628() {
	prepare_ralink_wifi mt7628
}

scan_mt7628() {
	scan_ralink_wifi mt7628 mt7628
}


disable_mt7628() {
	disable_ralink_wifi mt7628
}

enable_mt7628() {
	enable_ralink_wifi mt7628 mt7628
}

detect_mt7628() {
	local device=`cat /proc/net/dev | grep ra0`
	if [ -z "$device" ];then
		return
	fi

#	detect_ralink_wifi mt7628 mt7628
	ssid="TETON-2.4G"
	cd /sys/module/
	[ -d $module ] || return
	[ -e /etc/config/wireless ] && return
         cat <<EOF
config wifi-device      mt7628
        option type     mt7628
        option vendor   ralink
        option band     2.4G
        option channel  0
        option auotch   2
        option bw       1
        option country  CN
        option region  1
        option ht_bsscoexist 1
        option autoch_skip "12;13"

config wifi-iface	mt7628iface
        option device   mt7628
        option ifname   ra0
        option network  lan
        option mode     ap
        option ssid     $ssid
        option encryption none

EOF
}

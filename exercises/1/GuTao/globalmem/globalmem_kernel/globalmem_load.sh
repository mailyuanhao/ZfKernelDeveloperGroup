#!/bin/sh

module="globalmem"
device="globalmem"
mode="664"

/sbin/insmod ./$module.ko $* || exit 1

major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)

rm -f /dev/${device}
mknod /dev/${device} c $major 0
chmod $mode  /dev/${device}
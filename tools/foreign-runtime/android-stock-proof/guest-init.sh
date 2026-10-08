#!/bin/sh
# Test-only PID1, embedded in a reviewed fixture initramfs. Never run on a host.
set -eu
[ "$$" = 1 ] || exit 90
export PATH=/usr/bin:/usr/sbin:/bin:/sbin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
# QEMU is fixed with no NIC, GPU/audio or host filesystem exports.
[ "$(cat /sys/class/dmi/id/product_name)" = "Standard PC (Q35 + ICH9, 2009)" ] || exit 91
mkdir -p /run /tmp /var /sys/fs/cgroup
mount -t tmpfs -o mode=755,size=1G tmpfs /run
mount -t tmpfs -o mode=1777,size=1G tmpfs /tmp
mount -t tmpfs -o mode=755,size=3G tmpfs /var
mount -t cgroup2 none /sys/fs/cgroup
mkdir -p /dev/pts /dev/shm
mount -t devpts -o newinstance,ptmxmode=0666,mode=0620 devpts /dev/pts
mount -t tmpfs -o mode=1777,size=256M tmpfs /dev/shm
ln -sf pts/ptmx /dev/ptmx
# All module loading acts on this disposable kernel. Required modules and
# dependencies must be supplied by the matching Portage kernel input.
modprobe overlay
modprobe loop
modprobe ext4
mkdir -p /usr/share/waydroid-extra/images
[ -b /dev/vda ] && [ -b /dev/vdb ] || exit 92
[ "$(cat /sys/class/block/vda/ro)" = 1 ] || exit 93
[ "$(cat /sys/class/block/vdb/ro)" = 1 ] || exit 93
ln -s /dev/vda /usr/share/waydroid-extra/images/system.img
ln -s /dev/vdb /usr/share/waydroid-extra/images/vendor.img
mkdir -p /run/android-proof
exec /usr/bin/python3 /proof/guest.py

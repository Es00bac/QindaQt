#!/usr/bin/env python3
"""Pure argv for a fixture initramfs; does not build or install anything."""
from pathlib import Path
import json
import sys

KERNEL = "6.18.48-gentoo-dist-bin"
REQUIRED = (
    "usr/bin/python3", "usr/bin/bash", "usr/bin/mount", "usr/bin/modprobe",
    "usr/bin/dbus-daemon", "usr/bin/waydroid", "usr/bin/lxc-start",
    "usr/bin/lxc-stop", "usr/bin/lxc-info",
    "usr/bin/qindaqt-wm", "usr/bin/qindaqt-kwin",
    "usr/sbin/dnsmasq", "usr/sbin/nft", "usr/bin/ip",
    "usr/lib/dracut", "usr/lib/waydroid", "usr/lib/modules/" + KERNEL,
)
# Package closure is independently extracted/verified. No live /usr or host
# config gets exported. Runtime modules/data not found by ELF closure (Python,
# GI typelibs, Qt plugins, fonts, NSS) must be in this protected selected stage.
def build_argv(stage, fixture, empty_config, output):
    paths = [Path(p) for p in (stage, fixture, empty_config, output)]
    if any(not p.is_absolute() or ".." in p.parts for p in paths):
        raise ValueError("noncanonical-build-path")
    if len(set(paths)) != 4:
        raise ValueError("aliased-build-path")
    return ["/usr/bin/dracut", "--sysroot", str(stage),
            "--conf", "/dev/null", "--confdir", str(empty_config),
            "--no-hostonly", "--no-hostonly-cmdline", "--no-hostonly-default-device",
            "--modules", "base kernel-modules",
            "--add-drivers", "virtio_pci virtio_blk overlay loop ext4 veth bridge nf_tables",
            "--include", str(Path(stage)/"usr"), "/usr",
            "--include", str(fixture), "/",
            str(output), KERNEL]

if __name__ == "__main__":
    if len(sys.argv) != 5: raise SystemExit("stage fixture empty-config output")
    print(json.dumps(build_argv(*sys.argv[1:]), indent=2))

#!/usr/bin/env python3
"""Prepare only generated fixture files in a fresh directory; no boot or mount."""
import os
from pathlib import Path
import sys

def prepare(destination, source):
    destination.mkdir(mode=0o755)  # parent run directory remains private0700
    for name in ("proc", "sys", "dev", "run", "tmp", "var", "root", "home",
                 "etc", "etc/fonts", "proof", "usr", "usr/share",
                 "usr/share/waydroid-extra", "usr/share/waydroid-extra/images"):
        (destination/name).mkdir(parents=True, exist_ok=True)
        (destination/name).chmod(0o755)
    for name, target in (("bin", "usr/bin"), ("sbin", "usr/sbin"),
                         ("lib", "usr/lib"), ("lib64", "usr/lib64"), ("usr/sbin", "bin")):
        (destination/name).symlink_to(target)
    (destination/"usr/bin").mkdir()
    for name, target in (("python3", "python3.14"), ("sh", "bash"), ("awk", "gawk")):
        (destination/"usr/bin"/name).symlink_to(target)
    generated = {
        "etc/passwd": "root:x:0:0:root:/root:/bin/sh\nproof:x:1000:1000:proof:/home/proof:/bin/sh\nnobody:x:65534:65534:guest network helper:/nonexistent:/usr/sbin/nologin\n",
        "etc/group": "root:x:0:\nproof:x:1000:\nnobody:x:65534:\n",
        "etc/nsswitch.conf": "passwd: files\ngroup: files\nhosts: files\n",
        "etc/hosts": "127.0.0.1 localhost\n::1 localhost\n",
        "etc/fonts/fonts.conf": '<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd"><fontconfig><dir>/usr/share/fonts</dir><cachedir>/var/cache/fontconfig</cachedir></fontconfig>\n',
    }
    for name, body in generated.items():
        p = destination/name
        with p.open("x") as stream: stream.write(body)
        p.chmod(0o644)
    for name, target in (("guest-init.sh", "init"), ("guest.py", "proof/guest.py"),
                         ("windows.py", "proof/windows.py"), ("frame_capture.py", "proof/frame_capture.py"),
                         ("interaction.py", "proof/interaction.py"), ("scenario.json", "proof/scenario.json")):
        fd = os.open(source/name, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC)
        try:
            body = os.read(fd, 131073)
            if len(body) > 131072: raise ValueError("fixture-source-bound")
        finally: os.close(fd)
        p = destination/target
        with p.open("xb") as stream: stream.write(body)
        p.chmod(0o644 if name.endswith(".json") else 0o755)
    return destination

if __name__ == "__main__":
    if len(sys.argv) != 2: raise SystemExit("fresh-output-directory")
    prepare(Path(sys.argv[1]), Path(__file__).resolve().parent)

#!/usr/bin/env python3
"""Pure cpio build plan over an independently admitted prepared fixture root.

The stage contains only qualified selected Portage userspace/kernel-module
inputs plus generated fixture files. No package solving or host-root copying.
"""
from pathlib import Path
import json
import os
import stat
import sys

KERNEL = "6.18.48-gentoo-dist-bin"
TOP = {"usr", "bin", "sbin", "lib", "lib64", "etc", "init", "proof",
       "proc", "sys", "dev", "run", "tmp", "var", "root", "home"}
REQUIRED = ("init", "proof/guest.py", "proof/windows.py", "proof/scenario.json",
            "usr/bin/python3", "usr/bin/bash", "usr/bin/mount", "usr/bin/modprobe",
            "usr/bin/dbus-daemon", "usr/bin/waydroid", "usr/bin/lxc-start",
            "usr/bin/lxc-stop", "usr/bin/lxc-info", "usr/bin/qindaqt-wm",
            "usr/bin/qindaqt-kwin", "usr/lib/waydroid",
            "usr/lib/modules/" + KERNEL)

def archive_paths(root):
    """Readonly bounded inventory; symlinks are archived, never traversed."""
    root = Path(root)
    if root.is_symlink() or not root.is_dir():
        raise ValueError("stage-directory")
    if set(p.name for p in root.iterdir()) - TOP:
        raise ValueError("unexpected-root-entry")
    if (root/"usr").is_symlink():
        raise ValueError("symlink-userspace-root")
    result = ["."]; total = 0
    for parent, directories, files in os.walk(root, followlinks=False):
        for name in sorted(directories + files):
            path = Path(parent)/name
            relative = path.relative_to(root).as_posix()
            if any(c in relative for c in "\n\r\0"):
                raise ValueError("noncanonical-archive-name")
            info = path.lstat()
            if not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)
                    or stat.S_ISLNK(info.st_mode)):
                raise ValueError("special-stage-object")
            if info.st_mode & (stat.S_ISUID | stat.S_ISGID):
                raise ValueError("privileged-stage-mode")
            if stat.S_ISREG(info.st_mode): total += info.st_size
            result.append("./" + relative)
            if len(result) > 100000 or total > 6*1024**3:
                raise ValueError("stage-bound")
    return sorted(result)

def plan(root):
    paths = archive_paths(root)
    # Parent must freeze/recheck every staged byte before/after this separately
    # granted build. This inventory is shape validation, not package authority.
    return {"schema":1, "kind":"fixture-initramfs-build-plan", "cwd":str(Path(root).absolute()),
            "cpioArgv":["/usr/bin/cpio", "--null", "--create", "--format=newc",
                        "--owner=0:0", "--reproducible", "--quiet"],
            "stdinPaths":paths, "stdinEncoding":"UTF-8 paths separated by NUL",
            "gzipArgv":["/usr/bin/gzip", "-n", "-1"],
            "outputPolicy":"fresh exclusive raw then gzip files, retain failures",
            "runtimeAuthorized":False}

if __name__ == "__main__":
    if len(sys.argv) != 2: raise SystemExit("prepared-reviewed-fixture-root")
    print(json.dumps(plan(Path(sys.argv[1])), indent=2))

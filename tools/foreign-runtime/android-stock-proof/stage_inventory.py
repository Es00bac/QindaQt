"""Read-only stage admission against an independently qualified input inventory.

No package signatures are established here. The manager supplies the exact
inventory from separately verified package images and generated fixture files.
Symlink resolution is guest-rooted; host absolute targets are never followed.
"""
import hashlib
import os
from pathlib import Path, PurePosixPath
import stat

def staged_mode(name, source_mode):
    """Only the two reviewed util-linux helpers lose their installed setuid.

    This is a proposed stage-copy mode, not permission to modify host inputs.
    Other privileged objects remain refused by archive_paths; no broad strip.
    """
    if name in {"usr/bin/mount", "usr/bin/umount"} and source_mode == 0o4755:
        return 0o755
    if source_mode & 0o6000:
        raise ValueError("unreviewed-privileged-stage-input")
    return source_mode

def guest_target(root, relative):
    pending = list(PurePosixPath(relative).parts)
    resolved = []
    links = 0
    while pending:
        part = pending.pop(0)
        if part in ("", "."):
            continue
        if part == "..":
            if not resolved:
                raise ValueError("stage-link-escape")
            resolved.pop()
            continue
        candidate = root.joinpath(*resolved, part)
        info = candidate.lstat()
        if stat.S_ISLNK(info.st_mode):
            links += 1
            if links > 40:
                raise ValueError("stage-link-cycle")
            target = candidate.readlink()
            if target.is_absolute():
                resolved = []
            pending = list(target.parts[1:] if target.is_absolute() else target.parts) + pending
        else:
            if pending and not stat.S_ISDIR(info.st_mode):
                raise ValueError("stage-parent-not-directory")
            resolved.append(part)
    return root.joinpath(*resolved)

def admit(root, paths, inventory, required, directories):
    root = Path(root)
    if inventory.get("schema") != 1 or set(inventory) != {"schema", "objects"}:
        raise ValueError("stage-inventory-schema")
    objects = inventory["objects"]
    names = {p[2:] for p in paths if p != "."}
    if not isinstance(objects, dict) or set(objects) != names:
        raise ValueError("stage-inventory-coverage")
    for name, record in objects.items():
        # archive_paths already forbids special objects and privileged modes.
        path = root/name
        info = path.lstat()
        if record.get("mode") != stat.S_IMODE(info.st_mode):
            raise ValueError("stage-mode-changed")
        if not isinstance(record.get("input"), str) or not record["input"]:
            raise ValueError("stage-input-binding")
        if stat.S_ISREG(info.st_mode):
            if record.get("kind") != "file" or record.get("size") != info.st_size:
                raise ValueError("stage-file-shape")
            digest = hashlib.sha256()
            fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
            version = lambda st: (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns, st.st_ctime_ns)
            try:
                if version(os.fstat(fd)) != version(info):
                    raise ValueError("stage-file-replaced")
                remaining = info.st_size
                while remaining:
                    block = os.read(fd, min(1024*1024, remaining))
                    if not block:
                        raise ValueError("stage-file-truncated")
                    digest.update(block)
                    remaining -= len(block)
                if os.read(fd, 1) or version(os.fstat(fd)) != version(info):
                    raise ValueError("stage-file-changed")
                if version(path.lstat()) != version(info):
                    raise ValueError("stage-file-replaced")
            finally:
                os.close(fd)
            if digest.hexdigest() != record.get("sha256"):
                raise ValueError("stage-file-changed")
        elif stat.S_ISLNK(info.st_mode):
            if record.get("kind") != "symlink" or record.get("target") != str(path.readlink()):
                raise ValueError("stage-link-changed")
        elif record.get("kind") != "directory":
            raise ValueError("stage-directory-shape")
    admitted = {}
    for name in required:
        target = guest_target(root, name)
        info = target.lstat()
        if name in directories:
            if not stat.S_ISDIR(info.st_mode):
                raise ValueError("required-stage-directory")
        elif not stat.S_ISREG(info.st_mode):
            raise ValueError("required-stage-file")
        elif (name == "init" or name.startswith("usr/bin/")) and not info.st_mode & 0o111:
            raise ValueError("required-stage-executable")
        admitted[name] = target.relative_to(root).as_posix()
    return admitted

"""Read-only public symlink closure; never collapse intermediate VDB links."""
import hashlib
import os
from pathlib import Path
import stat


def public_name(name):
    """Only the explicitly admitted usr-merge aliases are normalized."""
    if not name.startswith("/"):
        raise ValueError("relative-public-input")
    # Linux treats a doubled leading slash as the same root, not a network
    # authority. Canonicalize it before the explicit usr-merge mapping.
    name = os.path.normpath("/" + name.lstrip("/"))
    for prefix in ("/bin/", "/sbin/", "/lib/", "/lib64/"):
        if name.startswith(prefix):
            name = "/usr" + name
            break
    if name.startswith("/usr/sbin/"):
        name = "/usr/bin/" + name[len("/usr/sbin/"):]
    if not name.startswith("/usr/") or any(c in name for c in "\n\r\0"):
        raise ValueError("outside-public-userspace")
    return name


def chain(start, owners, root=Path("/"), uid=0, limit=40):
    """Return every observed link and the final VDB-owned regular name.

    Caller collects the terminal bytes through its existing held descriptor
    hash/version gate. This function executes no target, follows no link via
    resolve(), and fails on cycle/depth/ownership/type/version disagreement.
    """
    current = public_name(start)
    seen = set()
    links = []
    version = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns,
                         s.st_ctime_ns, s.st_uid, s.st_mode)
    while True:
        if current in seen:
            raise ValueError("public-link-cycle")
        seen.add(current)
        path = root/current.lstrip("/")
        before = path.lstat()
        authority = owners.get(current)
        if not authority or before.st_uid != uid:
            raise ValueError("unowned-public-link-input")
        if stat.S_ISREG(before.st_mode):
            return links, current
        if not stat.S_ISLNK(before.st_mode):
            raise ValueError("special-public-link-target")
        if len(links) >= limit:
            raise ValueError("public-link-depth")
        target = os.readlink(path)
        if (stat.S_IMODE(before.st_mode) != 0o777
                or any(o.get("kind") != "symlink" or o.get("target") != target
                       for o in authority)
                or version(path.lstat()) != version(before)):
            raise ValueError("public-link-witness-changed")
        links.append((current, {
            "kind": "symlink", "target": target, "owners": authority,
            "sourceMode": 0o777,
            "targetSha256": hashlib.sha256(os.fsencode(target)).hexdigest()}))
        current = public_name(target if target.startswith("/")
                              else str(Path(current).parent/target))

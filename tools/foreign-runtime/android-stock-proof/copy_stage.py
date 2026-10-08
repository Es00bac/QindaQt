#!/usr/bin/env python3
"""Copy only a reviewed public inventory into a fresh protected fixture stage.

CLI runs only under a separate manager stage grant. It executes no payload,
archive tool, package manager or guest command. Failure retains the partial.
"""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import sys
import time

from boot_plan import archive_paths, plan as boot_plan
from prepare_overlay import prepare
from stage_inventory import admit, guest_target, staged_mode

FLAGS = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC
DIRECTORY = FLAGS | os.O_DIRECTORY
MAX_BYTES = 2 * 1024**3


def version(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def parts(name):
    path = PurePosixPath(name)
    if path.is_absolute() or path.as_posix() != name or any(
            part in ("", ".", "..") for part in path.parts) or not path.parts:
        raise ValueError("noncanonical-input-name")
    if any(c in name for c in "\n\r\0"):
        raise ValueError("input-name-control")
    return path.parts


def parent_fd(root_fd, name, create=False, expected=None):
    elements = parts(name)
    current = os.dup(root_fd)
    try:
        for index, element in enumerate(elements[:-1]):
            if create:
                try:
                    os.mkdir(element, 0o755, dir_fd=current)
                    made = os.open(element, DIRECTORY, dir_fd=current)
                    try:
                        os.fchmod(made, 0o755)
                    finally:
                        os.close(made)
                    if expected is not None:
                        expected["/".join(elements[:index+1])] = {
                            "kind": "directory", "mode": 0o755,
                            "input": "generated-parent"}
                except FileExistsError:
                    pass
            child = os.open(element, DIRECTORY, dir_fd=current)
            os.close(current)
            current = child
        return current, elements[-1]
    except BaseException:
        os.close(current)
        raise


def read_regular(root_fd, name, bound, expected_uid, expected_mode=None):
    parent, leaf = parent_fd(root_fd, name)
    fd = None
    try:
        before = os.stat(leaf, dir_fd=parent, follow_symlinks=False)
        fd = os.open(leaf, FLAGS, dir_fd=parent)
        held = os.fstat(fd)
        if (not stat.S_ISREG(held.st_mode) or held.st_uid != expected_uid
                or held.st_mode & 0o022 or held.st_size > bound
                or version(held) != version(before)):
            raise ValueError("input-file-admission")
        if expected_mode is not None and stat.S_IMODE(held.st_mode) != expected_mode:
            raise ValueError("source-mode-changed")
        result = bytearray()
        remaining = held.st_size
        while remaining:
            block = os.read(fd, min(1024*1024, remaining))
            if not block:
                raise ValueError("input-truncated")
            result.extend(block)
            remaining -= len(block)
        if (os.read(fd, 1) or version(os.fstat(fd)) != version(held)
                or version(os.stat(leaf, dir_fd=parent, follow_symlinks=False)) != version(held)):
            raise ValueError("input-changed")
        return bytes(result)
    finally:
        if fd is not None:
            os.close(fd)
        os.close(parent)


def vdb_check(vdb_fd, packages, uid):
    for cpv, record in packages.items():
        if len(parts(cpv)) != 2:
            raise ValueError("package-name")
        body = read_regular(vdb_fd, cpv + "/CONTENTS", 32*1024**2, uid)
        if hashlib.sha256(body).hexdigest() != record["contentsSha256"]:
            raise ValueError("vdb-changed")


def generated_inventory(root):
    expected = {}
    for entry in archive_paths(root):
        if entry == ".":
            continue
        name = entry[2:]
        path = root/name
        info = path.lstat()
        item = {"mode": stat.S_IMODE(info.st_mode), "input": "reviewed-overlay"}
        if stat.S_ISDIR(info.st_mode):
            item["kind"] = "directory"
        elif stat.S_ISLNK(info.st_mode):
            item.update(kind="symlink", target=str(path.readlink()))
        else:
            item.update(kind="file", size=info.st_size,
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        expected[name] = item
    return expected


def copy_objects(source_fd, stage_fd, objects, packages, expected, uid, deadline):
    total = 0
    if len(objects) > 20000:
        raise ValueError("input-count")
    for absolute, record in sorted(objects.items()):
        if time.monotonic() >= deadline:
            raise TimeoutError("stage-deadline")
        if not absolute.startswith("/usr/"):
            raise ValueError("nonpublic-input")
        name = absolute[1:]
        parts(name)
        if name in expected:
            raise ValueError("generated-input-collision")
        owners = record.get("owners", [])
        if not owners or any(owner.get("cpv") not in packages for owner in owners):
            raise ValueError("missing-package-owner")
        out_parent, leaf = parent_fd(stage_fd, name, True, expected)
        try:
            if record["kind"] == "file":
                size = record["size"]
                if type(size) is not int or size < 0 or size > 256*1024**2:
                    raise ValueError("input-size")
                total += size
                if total > MAX_BYTES:
                    raise ValueError("stage-byte-bound")
                body = read_regular(source_fd, name, size, uid, record["sourceMode"])
                digest = hashlib.sha256(body).hexdigest()
                md5 = hashlib.md5(body, usedforsecurity=False).hexdigest()
                mode = staged_mode(name, record["sourceMode"])
                if (len(body) != size or digest != record["sha256"]
                        or md5 != record["md5"] or mode != record["stageMode"]
                        or any(o.get("kind") != "file" or o.get("md5") != md5 for o in owners)):
                    raise ValueError("source-content-or-mode-changed")
                fd = os.open(leaf, os.O_WRONLY | os.O_CREAT | os.O_EXCL
                             | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600, dir_fd=out_parent)
                try:
                    view = memoryview(body)
                    while view:
                        written = os.write(fd, view)
                        if written <= 0:
                            raise OSError("short-stage-write")
                        view = view[written:]
                    os.fchmod(fd, mode)
                    if os.fstat(fd).st_nlink != 1:
                        raise ValueError("stage-hardlink")
                finally:
                    os.close(fd)
                expected[name] = {"kind": "file", "mode": mode, "size": size,
                                  "sha256": digest, "input": ",".join(o["cpv"] for o in owners)}
            elif record["kind"] == "symlink":
                parent, source_leaf = parent_fd(source_fd, name)
                try:
                    before = os.stat(source_leaf, dir_fd=parent, follow_symlinks=False)
                    target = os.readlink(source_leaf, dir_fd=parent)
                    if (not stat.S_ISLNK(before.st_mode) or before.st_uid != uid
                            or target != record["target"]
                            or any(o.get("kind") != "symlink" or o.get("target") != target for o in owners)
                            or version(os.stat(source_leaf, dir_fd=parent, follow_symlinks=False))
                            != version(before)):
                        raise ValueError("source-link-changed")
                finally:
                    os.close(parent)
                os.symlink(target, leaf, dir_fd=out_parent)
                expected[name] = {"kind": "symlink", "mode": 0o777, "target": target,
                                  "input": ",".join(o["cpv"] for o in owners)}
            else:
                raise ValueError("input-kind")
        finally:
            os.close(out_parent)
    return total


def write_json(path, value):
    with path.open("x") as stream:
        json.dump(value, stream, indent=2)
        stream.write("\n")


def main(run_root, plan_sha):
    """Root-only production entry; tests call pure/descriptor helpers explicitly."""
    run_root = Path(run_root)
    info = run_root.lstat()
    if (os.geteuid() != 0 or not stat.S_ISDIR(info.st_mode) or info.st_uid != 0
            or stat.S_IMODE(info.st_mode) != 0o700 or run_root.parent != Path("/var/tmp")
            or not run_root.name.startswith("qindaqt-android-laptop-stage-")):
        raise ValueError("protected-run-root")
    root_fd = os.open(run_root, DIRECTORY)
    source_fd = vdb_fd = stage_fd = None
    start = time.monotonic()
    deadline = start + 120
    result = {"success": False, "stageCreated": False, "runtimeAuthorized": False}
    primary = None
    try:
        body = read_regular(root_fd, "inputs/plan.json", 1024**2, 0)
        if hashlib.sha256(body).hexdigest() != plan_sha:
            raise ValueError("plan-pin")
        proposal = json.loads(body)
        if proposal["root"] != str(run_root) or proposal["resourceProfile"] != "laptop":
            raise ValueError("plan-domain")
        required_sources = {"copy_stage.py", "boot_plan.py", "stage_inventory.py",
                            "prepare_overlay.py", "guest-init.sh", "guest.py",
                            "windows.py", "scenario.json"}
        if not required_sources <= {parts(n)[-1] for n in proposal["sourcePins"]}:
            raise ValueError("incomplete-source-pins")
        for name, digest in proposal["sourcePins"].items():
            basename = parts(name)[-1]
            data = read_regular(root_fd, "inputs/source/" + basename, 1024**2, 0)
            if hashlib.sha256(data).hexdigest() != digest:
                raise ValueError("source-pin")
        if Path(__file__).resolve() != run_root/"inputs/source/copy_stage.py":
            raise ValueError("unprotected-copier")
        body = read_regular(root_fd, "inputs/inventory.json", 8*1024**2, 0)
        if hashlib.sha256(body).hexdigest() != proposal["inputInventory"]["sha256"]:
            raise ValueError("inventory-pin")
        inventory = json.loads(body)
        if (inventory["host"] != "qinda-top" or inventory["resourceProfile"] != "laptop"
                or inventory["issues"] or len(inventory["objects"]) != 6730):
            raise ValueError("inventory-domain")
        source_fd = os.open("/", DIRECTORY)
        vdb_fd = os.open("/var/db/pkg", DIRECTORY)
        vdb_check(vdb_fd, inventory["packages"], 0)
        stage = prepare(run_root/"stage", run_root/"inputs/source")
        result["stageCreated"] = True
        expected = generated_inventory(stage)
        stage_fd = os.open(stage, DIRECTORY)
        result["copiedBytes"] = copy_objects(source_fd, stage_fd, inventory["objects"],
                                               inventory["packages"], expected, 0, deadline)
        for name, item in expected.items():
            if item["kind"] == "symlink":
                guest_target(stage, name)
        complete = {"schema": 1, "objects": expected}
        paths = archive_paths(stage)
        admit(stage, paths, complete, (), set())
        plan = boot_plan(stage, complete)
        vdb_check(vdb_fd, inventory["packages"], 0)
        for name, digest in proposal["sourcePins"].items():
            data = read_regular(root_fd, "inputs/source/" + parts(name)[-1], 1024**2, 0)
            if hashlib.sha256(data).hexdigest() != digest:
                raise ValueError("final-source-pin")
        if hashlib.sha256(read_regular(root_fd, "inputs/inventory.json", 8*1024**2, 0)).hexdigest() != proposal["inputInventory"]["sha256"]:
            raise ValueError("final-inventory-pin")
        if time.monotonic() >= deadline:
            raise TimeoutError("stage-deadline")
        write_json(run_root/"stage-inventory.json", complete)
        write_json(run_root/"archive-plan.json", plan)
        if time.monotonic() >= deadline:
            raise TimeoutError("stage-deadline")
        result.update(success=True, objects=len(expected), required=plan["requiredEntries"])
    except BaseException as error:
        result["errorType"] = type(error).__name__
        primary = error
    finally:
        for fd in (stage_fd, vdb_fd, source_fd, root_fd):
            if fd is not None:
                try:
                    os.close(fd)
                except OSError as error:
                    result.setdefault("cleanupErrors", []).append(type(error).__name__)
                    result["success"] = False
                    if primary is None:
                        primary = error
        result["elapsedSeconds"] = time.monotonic()-start
        # No cleanup or payload execution on either outcome. Preserve partials.
        try:
            write_json(run_root/"stage-result.json", result)
        except BaseException as error:
            if primary is None:
                primary = error
    if primary is not None:
        raise primary
    if time.monotonic() >= deadline:
        raise TimeoutError("post-evidence-stage-deadline")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("protected-run-root exact-plan-sha256")
    main(sys.argv[1], sys.argv[2])

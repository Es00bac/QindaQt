#!/usr/bin/env python3
"""One manager-granted VM; no package installation or host mount."""
import hashlib
import json
import os
from pathlib import Path
import selectors
import signal
import stat
import subprocess
import sys
import time
from vm_plan import argv, admit_envelope, minimal_environment, IMAGE_HASHES, manifest_profile

cancelled = False
def cancel(*_):
    global cancelled
    cancelled = True

def version(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)

def inputs(manifest):
    if manifest.get("schema") != 1 or set(manifest.get("inputs", {})) != {
            "kernel", "initramfs", "system", "vendor"}:
        raise ValueError("input-manifest-shape")
    fds, versions = {}, {}
    try:
        for role, row in manifest["inputs"].items():
            if not Path(row["path"]).is_absolute(): raise ValueError("relative-input")
            fd = os.open(row["path"], os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK)
            fds[role] = fd
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_uid != 0 or info.st_mode & 0o222:
                raise ValueError("input-not-root-protected-regular-file")
            if info.st_size != row["size"] or not 0 < info.st_size <= 4 * 1024**3:
                raise ValueError("input-size")
            before = version(info); digest = hashlib.sha256()
            while chunk := os.read(fd, 1024**2): digest.update(chunk)
            os.lseek(fd, 0, os.SEEK_SET)
            if digest.hexdigest() != row["sha256"] or version(os.fstat(fd)) != before:
                raise ValueError("input-hash-or-version")
            if role in IMAGE_HASHES and digest.hexdigest() != IMAGE_HASHES[role]:
                raise ValueError("unreviewed-image")
            versions[role] = before
        return fds, versions
    except BaseException:
        for fd in fds.values(): os.close(fd)
        raise

def envelope(resource_profile="qinda"):
    rows = [x[3:] for x in Path("/proc/self/cgroup").read_text().splitlines()
            if x.startswith("0::")]
    if len(rows) != 1 or ".." in Path(rows[0]).parts: raise ValueError("cgroup-path")
    root = Path("/sys/fs/cgroup") / rows[0].lstrip("/")
    values = {key:(root/key).read_text().strip()
              for key in ("memory.max", "memory.swap.max", "pids.max", "cpu.max")}
    values["nice"] = os.getpriority(os.PRIO_PROCESS, 0)
    admit_envelope(values, os.sched_getaffinity(0), resource_profile)
    return values

def run(manifest_path, output):
    output.mkdir(mode=0o700)  # retain all partial evidence; never reuse a run
    manifest = json.loads(manifest_path.read_text())
    resource_profile = manifest_profile(manifest)
    limits = envelope(resource_profile)
    fds, versions = inputs(manifest)
    process = None; pidfd = None; primary = None; cleanup = None
    result = {"schema":1, "success":False, "qemuRetired":False,
              "innerCleanupQualified":False, "envelope":limits, "resourceProfile":resource_profile,
              "argv":argv(fds, resource_profile)}
    signal.signal(signal.SIGTERM, cancel); signal.signal(signal.SIGINT, cancel)
    start = time.monotonic()
    try:
        with (output/"serial.log").open("xb") as log:
            if cancelled: raise RuntimeError("cancelled-before-acquisition")
            process = subprocess.Popen(argv(fds, resource_profile), stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True,
                pass_fds=tuple(fds.values()), env=minimal_environment(), cwd=output)
            # The nonraising handler cannot interrupt acquisition. Until reaped,
            # this direct Popen remains owned if pidfd acquisition itself fails.
            pidfd = os.pidfd_open(process.pid)
            result["pid"] = process.pid
            result["starttime"] = Path("/proc/"+str(process.pid)+"/stat").read_text().rsplit(")",1)[1].split()[19]
            os.set_blocking(process.stdout.fileno(), False)
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                count = 0; eof = False
                while not eof:
                    if cancelled or time.monotonic()-start > 300:
                        raise RuntimeError("cancelled-or-deadline")
                    for key,_ in selector.select(.1):
                        data = os.read(key.fileobj.fileno(), 65536)
                        if not data: eof = True; break
                        count += len(data)
                        if count > 32*1024**2: raise RuntimeError("output-bound")
                        log.write(data)
            result["qemuExit"] = process.wait(timeout=2)
            if result["qemuExit"] != 0: raise RuntimeError("qemu-failed")
    except BaseException as error:
        primary = error
    finally:
        if process is not None:
            try:
                if process.poll() is None:
                    try:
                        if pidfd is None: process.terminate()
                        else: signal.pidfd_send_signal(pidfd, signal.SIGTERM)
                    except BaseException as error:
                        cleanup = error
                    try: process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        try:
                            if pidfd is None: process.kill()
                            else: signal.pidfd_send_signal(pidfd, signal.SIGKILL)
                        except BaseException as error:
                            if cleanup is None: cleanup = error
                        process.wait(timeout=2)
                result["qemuRetired"] = process.returncode is not None
            except BaseException as error:
                if cleanup is None: cleanup = error
        try:
            result["inputVersionsStable"] = all(version(os.fstat(fd)) == versions[role]
                                                for role,fd in fds.items())
        except BaseException as error:
            if cleanup is None: cleanup = error
            result["inputVersionsStable"] = False
        finally:
            for fd in fds.values(): os.close(fd)
            if pidfd is not None: os.close(pidfd)
        result["elapsedSeconds"] = time.monotonic()-start
        if primary: result["errorType"] = type(primary).__name__
        if cleanup: result["cleanupErrorType"] = type(cleanup).__name__
        if primary is None and cleanup is None and result["elapsedSeconds"] <= 300:
            prefix = b"QINDA_ANDROID_RESULT="
            rows = [line[len(prefix):] for line in (output/"serial.log").read_bytes().splitlines()
                    if line.startswith(prefix)]
            if len(rows) == 1:
                try:
                    guest = json.loads(rows[0]); result["guest"] = guest
                    result["innerCleanupQualified"] = guest.get("cleanupQualified") is True
                    result["success"] = (guest.get("success") is True and result["qemuRetired"]
                        and result["inputVersionsStable"] and result["innerCleanupQualified"]
                        and not cancelled)
                except (ValueError, TypeError): pass
        try: (output/"result.json").write_text(json.dumps(result, indent=2)+"\n")
        except BaseException:
            if primary is None and cleanup is None: raise
    if primary is not None: raise primary
    if cleanup is not None: raise cleanup
    # Final write/deadline/cancellation cannot turn a late receipt into success.
    return 0 if result["success"] and not cancelled and time.monotonic()-start <= 300 else 1

if __name__ == "__main__":
    if len(sys.argv) != 4 or sys.argv[1] != "--execute-manager-admitted":
        raise SystemExit("Requires exact input review and separate manager runtime grant")
    raise SystemExit(run(Path(sys.argv[2]), Path(sys.argv[3])))

#!/usr/bin/env python3
"""Fixed test-only KVM argv; no guest or process is launched by this module."""
import os
import re

AFFINITY = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17}
# Fixed named fixture profiles; manifest values never become arbitrary QEMU
# options. The laptop runs its native installed x86_64 closure under -cpu host.
PROFILES = {
    "qinda": {"cpus": 8, "guestMiB": 8192, "hostBytes": 12 * 1024**3,
              "affinity": frozenset(AFFINITY), "jobs": 8, "load": 8},
    "laptop": {"cpus": 4, "guestMiB": 4096, "hostBytes": 5 * 1024**3,
               "affinity": frozenset(range(8)), "jobs": 4, "load": 4},
}

def profile(name):
    if type(name) is not str or name not in PROFILES:
        raise ValueError("unknown-resource-profile")
    return PROFILES[name]

def manifest_profile(manifest):
    # Missing key preserves the reviewed qinda schema1 compatibility. A
    # laptop manifest must opt in explicitly; no auto-detected host policy.
    name = manifest.get("resourceProfile", "qinda")
    profile(name)
    return name
IMAGE_HASHES = {
    "system": "30c87ee44412fee4f50173716a6bb7c6941d3021853f0adab4a249da8a9455ab",
    "vendor": "5cc4267517ded28f1cf60a5d2b6e8b88b15d8520de1f55a39de7764e7929a877",
}

def argv(fds, resource_profile="qinda"):
    """FDs are separately held, hashed regular files, never caller QEMU options."""
    limits = profile(resource_profile)
    if set(fds) != {"kernel", "initramfs", "system", "vendor"}:
        raise ValueError("unexpected-input-set")
    if any(type(fd) is not int or fd < 3 for fd in fds.values()):
        raise ValueError("invalid-held-descriptor")
    if len(set(fds.values())) != 4:
        raise ValueError("aliased-input-descriptor")
    result = [
        "/usr/bin/qemu-system-x86_64", "-no-user-config", "-nodefaults",
        "-machine", "q35,accel=kvm", "-cpu", "host", "-smp", str(limits["cpus"]),
        "-m", str(limits["guestMiB"]), "-display", "none", "-monitor", "none", "-nic", "none",
        "-no-reboot", "-serial", "stdio",
        "-kernel", "/proc/self/fd/" + str(fds["kernel"]),
        "-initrd", "/proc/self/fd/" + str(fds["initramfs"]),
        "-append", "console=ttyS0 panic=1 rdinit=/init",
    ]
    for role in ("system", "vendor"):
        result += ["-drive", "file=/proc/self/fd/" + str(fds[role])
                   + ",format=raw,if=virtio,readonly=on,cache=none"]
    return result

def admit_envelope(values, affinity, resource_profile="qinda"):
    """Current owning cgroup observations, not requested systemd properties."""
    limits = profile(resource_profile)
    if not affinity or not set(affinity).issubset(limits["affinity"]):
        raise ValueError("unexpected-affinity")
    for key, limit in (("memory.max", limits["hostBytes"]),
                       ("memory.swap.max", 0), ("pids.max", 256)):
        value = values[key]
        if not re.fullmatch(r"[0-9]+", value) or int(value) > limit:
            raise ValueError("unbounded-" + key)
    cpu = values["cpu.max"].split()
    if len(cpu) != 2 or not all(re.fullmatch(r"[0-9]+", x) for x in cpu):
        raise ValueError("unbounded-cpu")
    quota, period = map(int, cpu)
    if quota <= 0 or period <= 0 or quota > period * limits["cpus"]:
        raise ValueError("excess-cpu")
    if values["nice"] < 10:
        raise ValueError("missing-low-priority")

def minimal_environment():
    return {"PATH": "/usr/bin", "HOME": "/nonexistent", "LANG": "C",
            "LC_ALL": "C", "DBUS_SESSION_BUS_ADDRESS": "unix:path=/nonexistent",
            "DBUS_SYSTEM_BUS_ADDRESS": "unix:path=/nonexistent"}

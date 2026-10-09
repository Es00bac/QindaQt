# SPDX-License-Identifier: GPL-3.0-or-later
"""Protected exact inputs and independent writable cache for one private cohort."""
from __future__ import annotations
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import stat

from artifact_receipt import ARCHIVE, MAX_FILE

MANIFEST = "21710d085d27abe8fa243ee15e4d269622914ccf5466c6e165a0736bd8050da4"
DISTFILES = {
    "pipewire-1.6.8.tar.bz2": ARCHIVE,
    "pipewire-1.6.0-docs.tar.xz":
        "9a907be0ee1fbce53272d9c8b99dbc95c4cab02b7053883cab1b65ead6b02b31",
}


def _version(value: os.stat_result) -> tuple[int, ...]:
    return (value.st_dev, value.st_ino, value.st_size, value.st_mtime_ns,
            value.st_ctime_ns, value.st_mode, value.st_uid, value.st_gid, value.st_nlink)


def _directory(path: Path) -> int:
    return os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC)


@dataclass(frozen=True)
class _Spec:
    name: str
    size: int
    hashes: tuple[tuple[str, str], ...]


def _observe(directory: int, spec: _Spec, destination: int | None = None) -> dict:
    """Hash a held regular FD and fence same-inode writes and named replacement."""
    descriptor = os.open(spec.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC
                         | os.O_NONBLOCK, dir_fd=directory)
    try:
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or before.st_size != spec.size:
            raise ValueError("distfile must be an exact-sized regular file")
        hashes = {name: hashlib.new(name) for name, _ in spec.hashes}
        size = 0
        while chunk := os.read(descriptor, min(65536, spec.size + 1 - size)):
            size += len(chunk)
            if size > spec.size:
                raise ValueError("distfile grew while reading")
            for digest in hashes.values():
                digest.update(chunk)
            if destination is not None:
                pending = memoryview(chunk)
                while pending:
                    written = os.write(destination, pending)
                    if written <= 0:
                        raise OSError("distfile snapshot write failed")
                    pending = pending[written:]
        after = os.fstat(descriptor)
        named = os.stat(spec.name, dir_fd=directory, follow_symlinks=False)
        if _version(before) != _version(after) or _version(after) != _version(named):
            raise ValueError("distfile changed during admission")
        if size != spec.size or any(hashes[name].hexdigest() != expected
                                   for name, expected in spec.hashes):
            raise ValueError("distfile does not match the reviewed Manifest")
        return {"sha256": hashes["sha256"].hexdigest(), "bytes": size,
                "version": list(_version(after))}
    finally:
        os.close(descriptor)


def _manifest(path: Path, digest: str, expected: dict[str, str]) -> tuple[bytes, tuple[_Spec, ...]]:
    directory = _directory(path.parent)
    try:
        info = os.stat(path.name, dir_fd=directory, follow_symlinks=False)
        if (not stat.S_ISREG(info.st_mode) or info.st_mode & 0o022
                or not 0 < info.st_size <= 65536):
            raise ValueError("Manifest is writable or outside the bounded contract")
        spec = _Spec(path.name, info.st_size, (("sha256", digest),))
        # The small Manifest is read separately only after a fenced digest;
        # a second held-FD observation below binds those actual parsed bytes.
        record = _observe(directory, spec)
        descriptor = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC
                             | os.O_NONBLOCK, dir_fd=directory)
        try:
            if list(_version(os.fstat(descriptor))) != record["version"]:
                raise ValueError("Manifest changed before parsing")
            content = bytearray()
            while chunk := os.read(descriptor, min(65536, spec.size + 1 - len(content))):
                content.extend(chunk)
                if len(content) > spec.size:
                    raise ValueError("Manifest grew before parsing")
            if (len(content) != spec.size or hashlib.sha256(content).hexdigest() != digest
                    or list(_version(os.fstat(descriptor))) != record["version"]
                    or list(_version(os.stat(path.name, dir_fd=directory,
                                             follow_symlinks=False))) != record["version"]):
                raise ValueError("Manifest changed before parsing")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)
    rows = bytes(content).decode("ascii").splitlines()
    if len(rows) != len(expected):
        raise ValueError("reviewed Manifest row count changed")
    result = {}
    for row in rows:
        fields = row.split()
        if (len(fields) != 7 or fields[0] != "DIST" or fields[1] not in expected
                or Path(fields[1]).name != fields[1] or fields[1] in {".", ".."}
                or fields[1] in result or fields[3::2] != ["BLAKE2B", "SHA512"]
                or not fields[2].isdecimal()):
            raise ValueError("invalid exact Manifest row")
        size = int(fields[2])
        if not 0 < size <= MAX_FILE:
            raise ValueError("distfile size outside the bounded input contract")
        result[fields[1]] = _Spec(fields[1], size, (
            ("sha256", expected[fields[1]]), ("blake2b", fields[4]), ("sha512", fields[6])))
    return bytes(content), tuple(result.values())


def _new_file(directory: int, name: str) -> int:
    return os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW
                   | os.O_CLOEXEC, 0o600, dir_fd=directory)


@dataclass(frozen=True)
class StagedInputs:
    """Private synchronous recipe state; caller protects the cohort until completion.

    Files are observed, not locked against a root writer. No Portage process
    receives the protected directory. Failure retains partial evidence and
    requires a fresh output; this helper never repairs an existing cohort.
    """
    snapshot: Path
    distdir: Path
    specs: tuple[_Spec, ...]
    manifest_spec: _Spec
    manifest_digest: str
    directory_version: tuple[int, ...]
    pinned: dict[str, dict]

    def verify(self) -> dict:
        snapshot = _directory(self.snapshot)
        cache = None
        try:
            cache = _directory(self.distdir)
            if (_version(os.fstat(snapshot)) != self.directory_version
                    or stat.S_IMODE(os.fstat(snapshot).st_mode) != 0o700):
                raise ValueError("protected input directory changed")
            records = {}
            for spec in (self.manifest_spec, *self.specs):
                protected = _observe(snapshot, spec)
                if protected != self.pinned[spec.name] or protected["version"][8] != 1:
                    raise ValueError("protected input identity changed")
                if spec == self.manifest_spec:
                    records[spec.name] = {"protected": protected}
                    continue
                writable = _observe(cache, spec)
                if (protected["version"][:2] == writable["version"][:2]
                        or writable["version"][8] != 1):
                    raise ValueError("Portage cache must be an independent copy")
                records[spec.name] = {"protected": protected, "portageCache": writable}
            return {"verified": True, "manifestSha256": self.manifest_digest,
                    "protectedInputRoot": str(self.snapshot), "distdir": str(self.distdir),
                    "files": records,
                    "scope": "stable observed bytes/identity; no concurrent-writer exclusion"}
        finally:
            if cache is not None:
                os.close(cache)
            os.close(snapshot)


def _stage_exact_inputs(manifest: Path, source: Path, output: Path, group: int,
                        manifest_digest: str, expected: dict[str, str]) -> StagedInputs:
    """Test-local pin injection; the production wrapper supplies only fixed pins."""
    content, distfiles = _manifest(manifest, manifest_digest, expected)
    snapshot = output / "source-inputs"
    cache = output / "distfiles"
    snapshot.mkdir(mode=0o700)
    snapshot.chmod(0o700)
    cache.mkdir(mode=0o770)
    os.chown(cache, -1, group)
    cache.chmod(0o770)
    source_fd = _directory(source)
    snapshot_fd = cache_fd = None
    try:
        snapshot_fd = _directory(snapshot)
        cache_fd = _directory(cache)
        descriptor = _new_file(snapshot_fd, "Manifest")
        try:
            pending = memoryview(content)
            while pending:
                written = os.write(descriptor, pending)
                if written <= 0:
                    raise OSError("Manifest snapshot write failed")
                pending = pending[written:]
            os.fchmod(descriptor, 0o400)
        finally:
            os.close(descriptor)
        for spec in distfiles:
            descriptor = _new_file(snapshot_fd, spec.name)
            try:
                _observe(source_fd, spec, descriptor)
                os.fchmod(descriptor, 0o400)
            finally:
                os.close(descriptor)
            descriptor = _new_file(cache_fd, spec.name)
            try:
                _observe(snapshot_fd, spec, descriptor)
                os.fchown(descriptor, -1, group)
                os.fchmod(descriptor, 0o664)
            finally:
                os.close(descriptor)
        specs = (_Spec("Manifest", len(content), (("sha256", manifest_digest),)), *distfiles)
        # Portage never gets this Manifest copy. Recheck its protected
        # identity separately from the distfiles in the writable cache.
        pinned = {spec.name: _observe(snapshot_fd, spec) for spec in specs}
        directory_version = _version(os.fstat(snapshot_fd))
    finally:
        if cache_fd is not None:
            os.close(cache_fd)
        if snapshot_fd is not None:
            os.close(snapshot_fd)
        os.close(source_fd)
    return StagedInputs(snapshot, cache, distfiles, specs[0], manifest_digest,
                        directory_version, pinned)


def stage_inputs(manifest: Path, source: Path, output: Path, group: int) -> StagedInputs:
    # AGENT-CONTRACT: package admission uses this root-only snapshot, whereas
    # Portage may normalize its separate 0664 cache. Never pass source/cache
    # inputs directly as DISTDIR or relax artifact_receipt.artifact().
    return _stage_exact_inputs(manifest, source, output, group, MANIFEST, DISTFILES)

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify one private gpkg through Portage, then compare its complete payload."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat

MAX_FILES = 50000
MAX_BYTES = 4 * 1024 * 1024 * 1024


def image_manifest(root: Path) -> dict:
    result = {}; total = 0
    for directory, folders, files in os.walk(root, followlinks=False):
        for name in sorted(folders + files):
            path = Path(directory) / name
            relative = str(path.relative_to(root))
            value = path.lstat()
            record = {"mode": stat.S_IMODE(value.st_mode)}
            if stat.S_ISLNK(value.st_mode):
                record.update(kind="symlink", target=os.readlink(path))
            elif stat.S_ISDIR(value.st_mode):
                record["kind"] = "directory"
            elif stat.S_ISREG(value.st_mode):
                total += value.st_size
                if total > MAX_BYTES:
                    raise ValueError("image exceeds bounded comparison")
                digest = hashlib.sha256()
                with path.open("rb") as source:
                    while chunk := source.read(65536): digest.update(chunk)
                record.update(kind="file", bytes=value.st_size, sha256=digest.hexdigest())
            else:
                raise ValueError("unexpected special file in package image")
            result[relative] = record
            if len(result) > MAX_FILES:
                raise ValueError("too many image entries")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--signed", action="store_true")
    args = parser.parse_args()
    # Portage's public decompress() first verifies the full gpkg manifest,
    # payload hashes and required signatures, then safely extracts to a private
    # directory. No ROOT merge, post-install hook, world or service action.
    import portage
    from portage.gpkg import gpkg
    settings = portage.config(clone=portage.settings)
    settings.features.discard("binpkg-ignore-signature")
    if args.signed: settings.features.add("binpkg-request-signature")
    target = args.output.resolve()
    if target.exists():
        raise ValueError("use a fresh private verification directory")
    target.mkdir(parents=True, mode=0o700)
    unpacked = target / "payload"; unpacked.mkdir(mode=0o700)
    package = gpkg(settings, gpkg_file=str(args.package.resolve(strict=True)),
                   verify_signature=args.signed)
    package.decompress(str(unpacked))
    retained = image_manifest(args.image.resolve(strict=True))
    decoded = image_manifest(unpacked)
    if retained != decoded:
        differences = sorted(key for key in retained.keys() | decoded.keys()
                             if retained.get(key) != decoded.get(key))
        (target / "mismatches.json").write_text(json.dumps(differences) + "\n")
        raise ValueError("gpkg and retained Portage payload differ")
    (target / "payload-manifest.json").write_text(json.dumps(decoded, sort_keys=True, indent=2) + "\n")
    checksum = hashlib.sha256()
    with args.package.open("rb") as source:
        while chunk := source.read(65536): checksum.update(chunk)
    (target / "result.json").write_text(json.dumps({
        "fullPortageVerification": True, "requiredSignature": args.signed,
        "matchedEntries": len(decoded), "packageSha256": checksum.hexdigest(),
        "payloadManifestSha256": hashlib.sha256(
            (target / "payload-manifest.json").read_bytes()).hexdigest(),
        "scope": "private gpkg/image; no installed-host or lifecycle inference"}, indent=2) + "\n")
    print("Portage full package verification and exact payload comparison passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

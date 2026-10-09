#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read exact Portage source/image files; never build, install or launch a graph."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import stat
import subprocess
from cohort_qualification import admit_qualification, admit_publication_inputs, QUALIFICATION_KEYS
from evidence_io import exact_record, json_value, read_evidence

ARCHIVE = "efbaedf80450e5b08bb3196a4ca7ea17850d4a71ec1a8ae321e8937a7c95779e"
ORIGINAL = "8b35c47c2d94707f7cba0efea40088e8f12bf24b3cfb973c2137eff80872fbe3"
PATCHED = "8a348844f3df801078fc0a0cb27f08231890a6db862084cfc8529b5667eed7ac"
PATCH = "23625878bbb2f7346f6eed7dec6a907435a178aa770b796e6870aaafd1528e22"
ORIGINAL_RECIPE = "bacba65b91d111195d332f3f4e755ad9a747f19a712bbc7159f6430dc849a5e0"
PATCHED_RECIPE = "c9af68ec33b7c8ff13ac519be2b2629b87e271bda0bb0a30f2660b58789f9c62"
OVERLAY = "b489a5f8ce215da9fad5f5f10db0adbfa22af2a4"
MAX_FILE = 128 * 1024 * 1024


def artifact(path: Path) -> dict:
    path = path.resolve(strict=True)
    before = path.stat()
    if not stat.S_ISREG(before.st_mode) or before.st_mode & 0o022:
        raise ValueError("artifact must be a regular file without group/world writes")
    if not 0 < before.st_size <= MAX_FILE:
        raise ValueError("artifact size outside bounded image contract")
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(65536):
            checksum.update(chunk)
    after = path.stat()
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (
        after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns
    ):
        raise ValueError("artifact changed while hashing")
    return {"path": str(path), "sha256": checksum.hexdigest(), "bytes": after.st_size}


def asan_needed(path: Path) -> None:
    readelf = subprocess.run(["/usr/bin/readelf", "--dynamic", str(path)], check=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if "libasan" not in readelf.stdout and "libclang_rt.asan" not in readelf.stdout:
        raise ValueError("both diagnostic Core and module must depend on AddressSanitizer")


def make_receipt(image: Path, source: Path, recipe: Path, archive: Path,
                 mode: str, libdir: str, sanitizer: Path | None, verification: Path,
                 qualification: dict, final_inputs: dict, *, allow_failed_old: bool = False) -> dict:
    if mode not in {"unpatched-asan-control", "patched-asan", "patched-normal"}:
        raise ValueError("unknown qualification mode")
    admit_qualification(qualification, mode, allow_failed_old=allow_failed_old)
    admit_publication_inputs(final_inputs, qualification)
    image = image.resolve(strict=True)
    if image == Path("/") or not image.is_dir():
        raise ValueError("use a retained Portage image, never the host root")
    if libdir not in {"usr/lib64", "usr/lib"}:
        raise ValueError("only the package native libdir is supported")
    proof = json.loads((verification / "result.json").read_text())
    manifest = verification / "payload-manifest.json"
    if not proof.get("fullPortageVerification") or proof.get("matchedEntries", 0) <= 0:
        raise ValueError("complete Portage package/image verification is required first")
    if mode == "patched-normal" and not proof.get("requiredSignature"):
        raise ValueError("normal image requires full required-signature verification")
    if artifact(manifest)["sha256"] != proof.get("payloadManifestSha256"):
        raise ValueError("verified payload manifest changed")
    old = mode == "unpatched-asan-control"
    if artifact(archive)["sha256"] != ARCHIVE:
        raise ValueError("upstream source archive changed")
    if artifact(source / "src/modules/module-loopback.c")["sha256"] != (ORIGINAL if old else PATCHED):
        raise ValueError("actual prepared Portage module source does not match reviewed cohort")
    if artifact(recipe)["sha256"] != (ORIGINAL_RECIPE if old else PATCHED_RECIPE):
        raise ValueError("actual Portage recipe changed")
    module = image / libdir / "pipewire-0.3/libpipewire-module-loopback.so"
    core = image / libdir / "libpipewire-0.3.so.0"
    paths = {
        "core": core, "module": module, "daemon": image / "usr/bin/pipewire",
        "policy": Path("/usr/bin/wireplumber"),
        "serverConfig": image / "usr/share/pipewire/pipewire.conf",
        "clientConfig": image / "usr/share/pipewire/client.conf",
        "policyConfig": Path("/usr/share/wireplumber/wireplumber.conf"),
    }
    records = {name: artifact(path) for name, path in paths.items()}
    for name in ("core", "module", "daemon", "serverConfig", "clientConfig"):
        if not Path(records[name]["path"]).is_relative_to(image):
            raise ValueError("package-image symlink escaped the retained image")
    if mode != "patched-normal":
        if sanitizer is None:
            raise ValueError("diagnostic image requires an exact sanitizer runtime")
        asan_needed(core.resolve())
        asan_needed(module.resolve())
        records["asanRuntime"] = artifact(sanitizer)
    return {
        "schema": 2, "mode": mode, **qualification, "finalInputVerification": final_inputs, "imageRoot": str(image), "packageVerification": proof,
        "package": "media-video/pipewire-1.6.8" + ("" if old else "-r1"),
        "overlayCandidate": "" if old else OVERLAY,
        "archiveSha256": ARCHIVE, "moduleSourceSha256": ORIGINAL if old else PATCHED,
        "recipeSha256": ORIGINAL_RECIPE if old else PATCHED_RECIPE,
        "patchSha256": "" if old else PATCH,
        "moduleDirectory": str(module.parent.resolve(strict=True)),
        "spaDirectory": str((image / libdir / "spa-0.2").resolve(strict=True)),
        "libraryDirectory": str(core.resolve(strict=True).parent),
        "artifacts": records,
        "scope": ("diagnostic-only original failed-suite image; no release qualification"
                  if qualification["diagnosticOnly"] else
                  "Portage-image and package round trip; not installed-host or audio acceptance"),
    }


def bootstrap_environment(receipt: dict, private_root: Path, *,
                          allow_failed_old: bool = False) -> dict[str, str]:
    """Explicit pre-exec composition; never mutate a worker's environment."""
    admit_qualification({key: receipt[key] for key in QUALIFICATION_KEYS}, receipt["mode"],
                        allow_failed_old=allow_failed_old)
    admit_publication_inputs(receipt["finalInputVerification"], receipt)
    root = private_root.resolve()
    artifacts = receipt["artifacts"]
    environment = {
        "PATH": "/usr/bin:/bin", "LANG": "C.UTF-8", "LC_ALL": "C.UTF-8",
        "PIPEWIRE_MODULE_DIR": receipt["moduleDirectory"],
        "SPA_PLUGIN_DIR": receipt["spaDirectory"],
        "LD_LIBRARY_PATH": receipt["libraryDirectory"],
        "LD_PRELOAD": artifacts.get("asanRuntime", {}).get("path", ""),
        "DBUS_SESSION_BUS_ADDRESS": f"unix:path={root}/absent-session",
        "DBUS_SYSTEM_BUS_ADDRESS": f"unix:path={root}/absent-system",
    }
    for key, folder in {
        "XDG_RUNTIME_DIR": "runtime", "XDG_CONFIG_HOME": "config",
        "XDG_CONFIG_DIRS": "config", "XDG_STATE_HOME": "state",
        "XDG_CACHE_HOME": "cache", "XDG_DATA_HOME": "data", "XDG_DATA_DIRS": "data",
    }.items():
        directory = root / folder
        directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        directory.chmod(0o700)
        environment[key] = str(directory)
    return environment


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("image", "source", "recipe", "archive", "output", "package-verification"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mode", choices=["unpatched-asan-control", "patched-asan", "patched-normal"],
                        required=True)
    parser.add_argument("--libdir", choices=["usr/lib64", "usr/lib"], default="usr/lib64")
    parser.add_argument("--asan-runtime", type=Path)
    parser.add_argument("--qualification-evidence", type=Path, required=True)
    parser.add_argument("--final-input-record", type=Path, required=True,
                        help="JSON path/sha256/bytes record of final verified input observation")
    parser.add_argument("--allow-failed-upstream-old-asan", action="store_true")
    args = parser.parse_args()
    receipt = make_receipt(args.image, args.source, args.recipe, args.archive,
                           args.mode, args.libdir, args.asan_runtime, args.package_verification,
                           json_value(read_evidence(args.qualification_evidence)),
                           json_value(read_evidence(args.final_input_record)),
                           allow_failed_old=args.allow_failed_upstream_old_asan)
    args.output.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    args.output.write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n")
    args.output.chmod(0o600)
    print("cohort receipt SHA256:", hashlib.sha256(args.output.read_bytes()).hexdigest())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

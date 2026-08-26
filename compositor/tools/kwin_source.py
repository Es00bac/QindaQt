"""Validate, fetch, and verify the exact KWin source used by QindaQt."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence


OFFICIAL_REPOSITORY = "https://invent.kde.org/plasma/kwin.git"
OBJECT_ID = re.compile(r"^[0-9a-f]{40}$")
COMPOSITOR_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = COMPOSITOR_ROOT / "upstream" / "kwin.json"
DEFAULT_SERIES = COMPOSITOR_ROOT / "patches" / "series.json"


class VerificationError(RuntimeError):
    """Pinned upstream metadata or fetched Git state failed verification."""


@dataclass(frozen=True)
class KWinPin:
    repository: str
    release: str
    ref: str
    tag_object: str
    commit: str
    tree: str


def _object(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise VerificationError(f"{location} must be an object")
    return value


def _string(value: Any, location: str) -> str:
    if not isinstance(value, str) or not value:
        raise VerificationError(f"{location} must be a non-empty string")
    return value


def load_pin(path: Path) -> KWinPin:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise VerificationError(f"{path}: {error}") from error
    root = _object(document, str(path))
    unknown_root = set(root) - {"schemaVersion", "upstream", "integrationContract"}
    if unknown_root:
        raise VerificationError(f"KWin manifest has unknown fields: {sorted(unknown_root)}")
    if root.get("schemaVersion") != 1:
        raise VerificationError("KWin manifest schemaVersion must be 1")
    upstream = _object(root.get("upstream"), "upstream")
    expected_upstream_fields = {
        "project", "repository", "release", "ref", "tagObject", "commit", "tree"
    }
    if set(upstream) != expected_upstream_fields:
        raise VerificationError("upstream fields do not match the schema")
    if upstream.get("project") != "KWin":
        raise VerificationError("upstream.project must be KWin")
    _object(root.get("integrationContract"), "integrationContract")
    pin = KWinPin(
        repository=_string(upstream.get("repository"), "upstream.repository"),
        release=_string(upstream.get("release"), "upstream.release"),
        ref=_string(upstream.get("ref"), "upstream.ref"),
        tag_object=_string(upstream.get("tagObject"), "upstream.tagObject"),
        commit=_string(upstream.get("commit"), "upstream.commit"),
        tree=_string(upstream.get("tree"), "upstream.tree"),
    )
    if pin.repository != OFFICIAL_REPOSITORY:
        raise VerificationError(f"repository must be the official KDE KWin URL: {OFFICIAL_REPOSITORY}")
    if pin.ref != f"refs/tags/v{pin.release}":
        raise VerificationError("upstream ref must be refs/tags/v<release>")
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", pin.release):
        raise VerificationError("upstream.release must be a three-part release version")
    for label, object_id in (("tagObject", pin.tag_object), ("commit", pin.commit), ("tree", pin.tree)):
        if not OBJECT_ID.fullmatch(object_id):
            raise VerificationError(f"upstream.{label} must be a lowercase 40-character Git object ID")
    return pin


def verify_patch_series(path: Path, pin: KWinPin) -> int:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise VerificationError(f"{path}: {error}") from error
    root = _object(document, str(path))
    if set(root) != {"schemaVersion", "upstreamCommit", "patches"}:
        raise VerificationError("patch series fields do not match the schema")
    if root.get("schemaVersion") != 1 or root.get("upstreamCommit") != pin.commit:
        raise VerificationError("patch series must use schemaVersion 1 and the pinned upstream commit")
    patches = root.get("patches")
    if not isinstance(patches, list):
        raise VerificationError("patches must be an array")
    patch_root = path.parent.resolve()
    seen: set[str] = set()
    for index, raw_patch in enumerate(patches):
        patch = _object(raw_patch, f"patches[{index}]")
        if set(patch) != {"path", "sha256"}:
            raise VerificationError(f"patches[{index}] fields do not match the schema")
        relative = _string(patch.get("path"), f"patches[{index}].path")
        expected_hash = _string(patch.get("sha256"), f"patches[{index}].sha256")
        if not re.fullmatch(r"[0-9a-f]{64}", expected_hash):
            raise VerificationError(f"patches[{index}].sha256 must be lowercase SHA-256")
        if relative in seen or not re.fullmatch(r"[0-9]{4}-[A-Za-z0-9._-]+\.patch", relative):
            raise VerificationError(f"patches[{index}].path must be a unique numbered patch filename")
        candidate = (patch_root / relative).resolve()
        if not candidate.is_relative_to(patch_root) or not candidate.is_file():
            raise VerificationError(f"patch file does not exist beneath patches/: {relative}")
        actual_hash = hashlib.sha256(candidate.read_bytes()).hexdigest()
        if actual_hash != expected_hash:
            raise VerificationError(f"patch hash mismatch for {relative}: {actual_hash}")
        seen.add(relative)
    return len(patches)


def _git(arguments: Sequence[str], *, working_directory: Path | None = None) -> str:
    executable = shutil.which("git")
    if executable is None:
        raise VerificationError("git is required for remote and checkout verification")
    completed = subprocess.run(
        [executable, *arguments],
        cwd=working_directory,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        message = completed.stderr.strip() or completed.stdout.strip()
        raise VerificationError(f"git {' '.join(arguments[:2])} failed: {message}")
    return completed.stdout.strip()


def check_remote(pin: KWinPin) -> None:
    output = _git(["ls-remote", pin.repository, pin.ref, f"{pin.ref}^{{}}"])
    refs = {line.split("\t", 1)[1]: line.split("\t", 1)[0] for line in output.splitlines() if "\t" in line}
    if refs.get(pin.ref) != pin.tag_object:
        raise VerificationError(f"remote tag object differs from manifest: {refs.get(pin.ref, 'missing')}")
    peeled = refs.get(f"{pin.ref}^{{}}", refs.get(pin.ref))
    if peeled != pin.commit:
        raise VerificationError(f"remote release commit differs from manifest: {peeled}")


def verify_checkout(directory: Path, pin: KWinPin, *, require_clean: bool = True) -> None:
    if not (directory / ".git").exists():
        raise VerificationError(f"not a Git checkout: {directory}")
    commit = _git(["rev-parse", "HEAD^{commit}"], working_directory=directory)
    tree = _git(["rev-parse", "HEAD^{tree}"], working_directory=directory)
    if commit != pin.commit or tree != pin.tree:
        raise VerificationError(f"checkout mismatch: commit={commit}, tree={tree}")
    if require_clean and _git(["status", "--porcelain"], working_directory=directory):
        raise VerificationError("checkout has local modifications or untracked files")


def fetch_checkout(destination: Path, pin: KWinPin) -> None:
    target = destination.resolve()
    if target.exists():
        raise VerificationError(f"fetch destination must not already exist: {target}")
    if not target.parent.is_dir():
        raise VerificationError(f"fetch destination parent does not exist: {target.parent}")
    temporary = Path(tempfile.mkdtemp(prefix=f".{target.name}.partial-", dir=target.parent))
    try:
        _git(["init", "--quiet"], working_directory=temporary)
        _git(["remote", "add", "origin", pin.repository], working_directory=temporary)
        _git(["fetch", "--quiet", "--depth=1", "--no-tags", "origin", pin.commit],
             working_directory=temporary)
        _git(["-c", "advice.detachedHead=false", "checkout", "--quiet", "--detach", "FETCH_HEAD"],
             working_directory=temporary)
        verify_checkout(temporary, pin)
        temporary.rename(target)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Verify or fetch QindaQt's pinned upstream KWin source.")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--series", type=Path, default=DEFAULT_SERIES)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--check-remote", action="store_true")
    action.add_argument("--fetch", type=Path, metavar="NEW_DIRECTORY")
    action.add_argument("--verify", type=Path, metavar="CHECKOUT")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        pin = load_pin(arguments.manifest.resolve())
        patch_count = verify_patch_series(arguments.series.resolve(), pin)
        if arguments.check_remote:
            check_remote(pin)
        elif arguments.fetch:
            fetch_checkout(arguments.fetch, pin)
        elif arguments.verify:
            verify_checkout(arguments.verify.resolve(), pin)
    except VerificationError as error:
        print(f"verify-kwin-source: error: {error}", file=sys.stderr)
        return 1
    print(f"Verified KWin {pin.release} at {pin.commit} with {patch_count} downstream patches.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

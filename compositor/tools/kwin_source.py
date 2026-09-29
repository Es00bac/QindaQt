"""Validate, fetch, and verify the exact qindaqt-kwin source used by QindaQt.

QindaQt runs on qindaqt-kwin, its own co-installable fork of KWin (ADR-0289).
compositor/upstream/kwin.json pins the fork commit and the upstream KWin
release it descends from; this tool checks that pin against Git and against
the source archive the Gentoo package builds from.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence


OFFICIAL_REPOSITORY = "https://invent.kde.org/plasma/kwin.git"
FORK_PROJECT = "qindaqt-kwin"
OBJECT_ID = re.compile(r"^[0-9a-f]{40}$")
COMPOSITOR_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = COMPOSITOR_ROOT / "upstream" / "kwin.json"


class VerificationError(RuntimeError):
    """Pinned fork metadata, fetched Git state or the source archive failed verification."""


@dataclass(frozen=True)
class UpstreamPin:
    repository: str
    release: str
    ref: str
    tag_object: str
    commit: str
    tree: str


@dataclass(frozen=True)
class ForkPin:
    repository: str
    branch: str
    commit: str
    tree: str
    version: str
    package: str
    upstream: UpstreamPin


def _object(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise VerificationError(f"{location} must be an object")
    return value


def _string(value: Any, location: str) -> str:
    if not isinstance(value, str) or not value:
        raise VerificationError(f"{location} must be a non-empty string")
    return value


def _object_id(value: str, location: str) -> str:
    if not OBJECT_ID.fullmatch(value):
        raise VerificationError(f"{location} must be a lowercase 40-character Git object ID")
    return value


def load_pin(path: Path) -> ForkPin:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise VerificationError(f"{path}: {error}") from error
    root = _object(document, str(path))
    unknown_root = set(root) - {"schemaVersion", "fork", "upstream", "integrationContract"}
    if unknown_root:
        raise VerificationError(f"compositor manifest has unknown fields: {sorted(unknown_root)}")
    if root.get("schemaVersion") != 2:
        raise VerificationError("compositor manifest schemaVersion must be 2 (the qindaqt-kwin fork)")

    upstream = _object(root.get("upstream"), "upstream")
    if set(upstream) != {"project", "repository", "release", "ref", "tagObject", "commit", "tree"}:
        raise VerificationError("upstream fields do not match the schema")
    if upstream.get("project") != "KWin":
        raise VerificationError("upstream.project must be KWin")
    upstream_pin = UpstreamPin(
        repository=_string(upstream.get("repository"), "upstream.repository"),
        release=_string(upstream.get("release"), "upstream.release"),
        ref=_string(upstream.get("ref"), "upstream.ref"),
        tag_object=_object_id(_string(upstream.get("tagObject"), "upstream.tagObject"), "upstream.tagObject"),
        commit=_object_id(_string(upstream.get("commit"), "upstream.commit"), "upstream.commit"),
        tree=_object_id(_string(upstream.get("tree"), "upstream.tree"), "upstream.tree"),
    )
    if upstream_pin.repository != OFFICIAL_REPOSITORY:
        raise VerificationError(f"upstream.repository must be the official KDE KWin URL: {OFFICIAL_REPOSITORY}")
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", upstream_pin.release):
        raise VerificationError("upstream.release must be a three-part release version")
    if upstream_pin.ref != f"refs/tags/v{upstream_pin.release}":
        raise VerificationError("upstream.ref must be refs/tags/v<release>")

    fork = _object(root.get("fork"), "fork")
    if set(fork) != {"project", "repository", "branch", "commit", "tree", "version", "package"}:
        raise VerificationError("fork fields do not match the schema")
    if fork.get("project") != FORK_PROJECT:
        raise VerificationError(f"fork.project must be {FORK_PROJECT}")
    pin = ForkPin(
        repository=_string(fork.get("repository"), "fork.repository"),
        branch=_string(fork.get("branch"), "fork.branch"),
        commit=_object_id(_string(fork.get("commit"), "fork.commit"), "fork.commit"),
        tree=_object_id(_string(fork.get("tree"), "fork.tree"), "fork.tree"),
        version=_string(fork.get("version"), "fork.version"),
        package=_string(fork.get("package"), "fork.package"),
        upstream=upstream_pin,
    )
    # AGENT-GUARD: the fork version is <upstream release>.<serial>, and the
    # Gentoo package is <release>_p<serial>; src/compositor's find_package(EXACT)
    # and the plugin interface id use the same version.
    match = re.fullmatch(re.escape(upstream_pin.release) + r"\.([1-9][0-9]*)", pin.version)
    if not match:
        raise VerificationError("fork.version must be <upstream.release>.<serial>")
    expected_package = f"gui-wm/{FORK_PROJECT}-{upstream_pin.release}_p{match.group(1)}"
    if pin.package != expected_package:
        raise VerificationError(f"fork.package must be {expected_package}")
    _object(root.get("integrationContract"), "integrationContract")
    return pin


def _git(arguments: Sequence[str], *, working_directory: Path | None = None, check: bool = True) -> str:
    executable = shutil.which("git")
    if executable is None:
        raise VerificationError("git is required for remote, checkout and archive verification")
    completed = subprocess.run(
        [executable, *arguments],
        cwd=working_directory,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if check and completed.returncode != 0:
        message = completed.stderr.strip() or completed.stdout.strip()
        raise VerificationError(f"git {' '.join(arguments[:2])} failed: {message}")
    return completed.stdout.strip() if completed.returncode == 0 else ""


def check_remote(pin: ForkPin, fork_repository: str | None = None) -> None:
    upstream = pin.upstream
    output = _git(["ls-remote", upstream.repository, upstream.ref, f"{upstream.ref}^{{}}"])
    refs = {line.split("\t", 1)[1]: line.split("\t", 1)[0] for line in output.splitlines() if "\t" in line}
    if refs.get(upstream.ref) != upstream.tag_object:
        raise VerificationError(f"remote tag object differs from manifest: {refs.get(upstream.ref, 'missing')}")
    peeled = refs.get(f"{upstream.ref}^{{}}", refs.get(upstream.ref))
    if peeled != upstream.commit:
        raise VerificationError(f"remote release commit differs from manifest: {peeled}")
    repository = fork_repository or pin.repository
    branch = _git(["ls-remote", repository, f"refs/heads/{pin.branch}"])
    if not branch:
        raise VerificationError(f"fork branch {pin.branch} is missing from {repository}")


def verify_checkout(directory: Path, pin: ForkPin, *, require_clean: bool = True) -> None:
    if not (directory / ".git").exists():
        raise VerificationError(f"not a Git checkout: {directory}")
    commit = _git(["rev-parse", "HEAD^{commit}"], working_directory=directory)
    tree = _git(["rev-parse", "HEAD^{tree}"], working_directory=directory)
    if commit != pin.commit or tree != pin.tree:
        raise VerificationError(f"checkout mismatch: commit={commit}, tree={tree}")
    if _git(["cat-file", "-t", pin.upstream.commit], working_directory=directory, check=False) != "commit":
        raise VerificationError("the checkout lacks the upstream KWin history the fork must keep")
    ancestor = subprocess.run(
        ["git", "merge-base", "--is-ancestor", pin.upstream.commit, pin.commit],
        cwd=directory, check=False, capture_output=True,
    )
    if ancestor.returncode != 0:
        raise VerificationError(f"fork commit does not descend from upstream KWin {pin.upstream.commit}")
    if require_clean and _git(["status", "--porcelain"], working_directory=directory):
        raise VerificationError("checkout has local modifications or untracked files")


def verify_archive(archive: Path, checkout: Path, pin: ForkPin) -> int:
    """Checks that a source archive holds exactly the pinned fork tree."""
    listing = _git(["ls-tree", "-r", "-z", "--full-tree", pin.commit], working_directory=checkout)
    expected: dict[str, str] = {}
    for entry in listing.split("\0"):
        if not entry:
            continue
        meta, name = entry.split("\t", 1)
        mode, kind, object_id = meta.split()
        if kind == "blob":
            expected[name] = object_id
    actual: dict[str, str] = {}
    try:
        with tarfile.open(archive) as bundle:
            for member in bundle.getmembers():
                if member.isdir() or member.name.endswith("pax_global_header"):
                    continue
                parts = member.name.split("/", 1)
                if len(parts) != 2 or not parts[1]:
                    raise VerificationError(f"archive entry outside the prefix directory: {member.name}")
                name = parts[1]
                if member.issym():
                    content = member.linkname.encode()
                elif member.isfile():
                    handle = bundle.extractfile(member)
                    content = handle.read() if handle else b""
                else:
                    raise VerificationError(f"unexpected archive entry type: {member.name}")
                header = f"blob {len(content)}\0".encode()
                actual[name] = hashlib.sha1(header + content).hexdigest()
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"{archive}: {error}") from error
    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    changed = sorted(name for name in set(expected) & set(actual) if expected[name] != actual[name])
    if missing or extra or changed:
        raise VerificationError(
            f"archive differs from fork commit {pin.commit}: {len(missing)} missing, {len(extra)} extra, "
            f"{len(changed)} changed (first: {(missing + extra + changed)[0]})")
    return len(actual)


def fetch_checkout(destination: Path, pin: ForkPin, fork_repository: str | None = None) -> None:
    target = destination.resolve()
    if target.exists():
        raise VerificationError(f"fetch destination must not already exist: {target}")
    if not target.parent.is_dir():
        raise VerificationError(f"fetch destination parent does not exist: {target.parent}")
    temporary = Path(tempfile.mkdtemp(prefix=f".{target.name}.partial-", dir=target.parent))
    try:
        _git(["init", "--quiet"], working_directory=temporary)
        _git(["remote", "add", "origin", fork_repository or pin.repository], working_directory=temporary)
        _git(["fetch", "--quiet", "--no-tags", "origin", pin.branch], working_directory=temporary)
        _git(["-c", "advice.detachedHead=false", "checkout", "--quiet", "--detach", pin.commit],
             working_directory=temporary)
        verify_checkout(temporary, pin)
        temporary.rename(target)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Verify or fetch QindaQt's pinned qindaqt-kwin source.")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--check-remote", action="store_true")
    action.add_argument("--fetch", type=Path, metavar="NEW_DIRECTORY")
    action.add_argument("--verify", type=Path, metavar="CHECKOUT")
    action.add_argument("--verify-archive", type=Path, metavar="ARCHIVE")
    parser.add_argument("--checkout", type=Path, help="fork checkout holding the pinned commit (--verify-archive)")
    # On qinda itself the hub is a local path rather than qinda:git/qindaqt-kwin.git.
    parser.add_argument("--fork-repository", help="fork repository to use instead of the manifest's (--check-remote, --fetch)")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        pin = load_pin(arguments.manifest.resolve())
        detail = ""
        if arguments.check_remote:
            check_remote(pin, arguments.fork_repository)
        elif arguments.fetch:
            fetch_checkout(arguments.fetch, pin, arguments.fork_repository)
        elif arguments.verify:
            verify_checkout(arguments.verify.resolve(), pin)
        elif arguments.verify_archive:
            if not arguments.checkout:
                raise VerificationError("--verify-archive needs --checkout <fork checkout>")
            count = verify_archive(arguments.verify_archive.resolve(), arguments.checkout.resolve(), pin)
            detail = f"; archive holds the same {count} files"
    except VerificationError as error:
        print(f"verify-kwin-source: error: {error}", file=sys.stderr)
        return 1
    print(f"Verified qindaqt-kwin {pin.version} at {pin.commit} on KWin {pin.upstream.release}{detail}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

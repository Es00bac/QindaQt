# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate local Markdown links and MkDocs navigation using only Python stdlib."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence
from urllib.parse import unquote, urlsplit


MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
MKDOCS_DIRECTORY = re.compile(r"^docs_dir:\s*(\S.*?)\s*$")
# Greedy label matching is required for quoted ADR labels such as "ADR-0001: ...".
MKDOCS_NAV_ENTRY = re.compile(r"^\s*-\s+.+:\s*([^#]+\.md)\s*$")
EXTERNAL_SCHEMES = {"app", "data", "file", "ftp", "http", "https", "mailto", "ssh"}


@dataclass(frozen=True)
class DocumentationIssue:
    source: Path
    line: int
    message: str


def _markdown_files(root: Path) -> list[Path]:
    candidates = list(root.glob("README*.md"))
    for directory_name in ("docs", "wiki"):
        directory = root / directory_name
        if directory.is_dir():
            candidates.extend(directory.rglob("*.md"))
    return sorted(set(path.resolve() for path in candidates if path.is_file()))


def _link_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and ">" in target:
        return target[1 : target.index(">")]
    # Markdown's optional title is outside the URL. Project paths do not contain
    # spaces, so this conservative split avoids treating titles as filenames.
    return target.split(maxsplit=1)[0]


def _validate_markdown(path: Path) -> list[DocumentationIssue]:
    issues: list[DocumentationIssue] = []
    in_fence = False
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeDecodeError) as error:
        return [DocumentationIssue(path, 0, f"cannot read UTF-8 Markdown: {error}")]
    for line_number, line in enumerate(lines, start=1):
        stripped = line.lstrip()
        if stripped.startswith(("```", "~~~")):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for match in MARKDOWN_LINK.finditer(line):
            target = _link_target(match.group(1))
            if not target or target.startswith("#"):
                continue
            parsed = urlsplit(target)
            if parsed.scheme.lower() in EXTERNAL_SCHEMES or target.startswith("/"):
                continue
            local_path = unquote(parsed.path)
            if not local_path:
                continue
            resolved = (path.parent / local_path).resolve()
            if not resolved.exists():
                issues.append(DocumentationIssue(path, line_number, f"broken local link: {target}"))
    if in_fence:
        issues.append(DocumentationIssue(path, len(lines), "unclosed fenced code block"))
    return issues


def _validate_mkdocs(root: Path) -> list[DocumentationIssue]:
    config_path = root / "mkdocs.yml"
    if not config_path.is_file():
        return [DocumentationIssue(config_path, 0, "mkdocs.yml is missing")]
    lines = config_path.read_text(encoding="utf-8").splitlines()
    docs_directory = root / "docs"
    for line in lines:
        match = MKDOCS_DIRECTORY.match(line)
        if match:
            docs_directory = (root / match.group(1).strip("'\"")).resolve()
            break
    issues: list[DocumentationIssue] = []
    nav_targets: set[Path] = set()
    for line_number, line in enumerate(lines, start=1):
        match = MKDOCS_NAV_ENTRY.match(line)
        if not match:
            continue
        target = (docs_directory / match.group(1).strip("'\"")).resolve()
        if target in nav_targets:
            issues.append(DocumentationIssue(config_path, line_number, f"duplicate nav target: {target.name}"))
        nav_targets.add(target)
        if not target.is_file():
            issues.append(DocumentationIssue(config_path, line_number, f"missing nav target: {target}"))
    if not nav_targets:
        issues.append(DocumentationIssue(config_path, 0, "no Markdown nav entries were found"))
    return issues


def validate(root: Path) -> tuple[list[Path], list[DocumentationIssue]]:
    documents = _markdown_files(root)
    issues: list[DocumentationIssue] = []
    if not documents:
        issues.append(DocumentationIssue(root, 0, "no Markdown documentation found"))
    for document in documents:
        issues.extend(_validate_markdown(document))
    issues.extend(_validate_mkdocs(root))
    return documents, issues


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate QindaQt wiki links and MkDocs navigation.")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    arguments = parser.parse_args(argv)
    documents, issues = validate(arguments.root.resolve())
    for issue in issues:
        source = issue.source.relative_to(arguments.root.resolve()) if issue.source.is_relative_to(arguments.root.resolve()) else issue.source
        print(f"{source}:{issue.line}: {issue.message}", file=sys.stderr)
    if issues:
        return 1
    print(f"Validated {len(documents)} Markdown documents and mkdocs.yml navigation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

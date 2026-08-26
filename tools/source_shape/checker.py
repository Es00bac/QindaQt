# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply size and cohesion proxies to hand-written source files."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path

from .config import ShapeConfig
from .language import SourceParseError, function_spans


@dataclass(frozen=True)
class Issue:
    severity: str
    path: str
    rule: str
    message: str

    def as_dict(self) -> dict[str, str]:
        return asdict(self)


@dataclass(frozen=True)
class FileShape:
    path: str
    nonblank_lines: int
    physical_lines: int
    byte_count: int


@dataclass(frozen=True)
class CheckReport:
    checked_files: int
    skipped_files: int
    issues: tuple[Issue, ...]
    largest_files: tuple[FileShape, ...]

    @property
    def errors(self) -> tuple[Issue, ...]:
        return tuple(issue for issue in self.issues if issue.severity == "error")


def _policy_extension(candidate: Path, config: ShapeConfig) -> str | None:
    if candidate.name == "CMakeLists.txt" and ".cmake" in config.extensions:
        return ".cmake"
    if candidate.suffix in config.extensions:
        return candidate.suffix
    if candidate.suffix or not candidate.stat().st_mode & 0o111:
        return None
    try:
        with candidate.open("rb") as stream:
            shebang = stream.readline(160).decode("ascii", errors="ignore")
    except OSError:
        return None
    if "python" in shebang and ".py" in config.extensions:
        return ".py"
    if any(shell in shebang for shell in ("/sh", "/bash", "/zsh")) and ".sh" in config.extensions:
        return ".sh"
    return None


def _source_paths(root: Path, config: ShapeConfig) -> list[tuple[Path, str]]:
    paths: list[tuple[Path, str]] = []
    for candidate in root.rglob("*"):
        if not candidate.is_file() or candidate.is_symlink():
            continue
        policy_extension = _policy_extension(candidate, config)
        if policy_extension is None:
            continue
        relative = candidate.relative_to(root).as_posix()
        if not config.ignores(relative):
            paths.append((candidate, policy_extension))
    return sorted(paths, key=lambda item: item[0])


def _check_file(
    path: Path,
    policy_extension: str,
    root: Path,
    config: ShapeConfig,
) -> tuple[FileShape, list[Issue], bool]:
    relative = path.relative_to(root).as_posix()
    limits, exception_reason, skip = config.policy_for(relative, policy_extension)
    if skip:
        return FileShape(relative, 0, 0, 0), [], True
    raw = path.read_bytes()
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        issue = Issue("error", relative, "utf8", f"source is not valid UTF-8: {error}")
        return FileShape(relative, 0, 0, len(raw)), [issue], False
    lines = text.splitlines()
    nonblank = sum(bool(line.strip()) for line in lines)
    shape = FileShape(relative, nonblank, len(lines), len(raw))
    issues: list[Issue] = []
    suffix = f" (allowlisted: {exception_reason})" if exception_reason else ""
    if nonblank > limits.max_nonblank_lines:
        issues.append(
            Issue(
                "error",
                relative,
                "file-lines",
                f"{nonblank} non-blank lines exceeds {limits.max_nonblank_lines}{suffix}",
            )
        )
    elif nonblank >= limits.review_nonblank_lines:
        issues.append(
            Issue(
                "warning",
                relative,
                "decomposition-review",
                f"{nonblank} non-blank lines reached review threshold {limits.review_nonblank_lines}{suffix}",
            )
        )
    if len(raw) > limits.max_bytes:
        issues.append(Issue("error", relative, "file-bytes", f"{len(raw)} bytes exceeds {limits.max_bytes}{suffix}"))
    longest_line = max((len(line) for line in lines), default=0)
    if longest_line > limits.max_line_characters:
        issues.append(
            Issue(
                "error",
                relative,
                "minified-line",
                f"longest line has {longest_line} characters; maximum is {limits.max_line_characters}{suffix}",
            )
        )
    try:
        spans = function_spans(path, text)
    except SourceParseError as error:
        issues.append(Issue("error", relative, "parse", str(error)))
        spans = []
    for span in spans:
        if span.line_count > limits.max_function_lines:
            issues.append(
                Issue(
                    "error",
                    relative,
                    "function-lines",
                    f"{span.name} spans {span.line_count} lines at line {span.start_line}; "
                    f"maximum is {limits.max_function_lines}{suffix}",
                )
            )
    return shape, issues, False


def check_repository(root: Path, config: ShapeConfig, largest_count: int = 10) -> CheckReport:
    """Check tracked source-shaped files without consulting Git or build tooling."""
    resolved_root = root.resolve()
    issues: list[Issue] = []
    shapes: list[FileShape] = []
    skipped = 0
    for path, policy_extension in _source_paths(resolved_root, config):
        try:
            shape, file_issues, was_skipped = _check_file(path, policy_extension, resolved_root, config)
        except OSError as error:
            relative = path.relative_to(resolved_root).as_posix()
            issues.append(Issue("error", relative, "read", str(error)))
            continue
        if was_skipped:
            skipped += 1
            continue
        shapes.append(shape)
        issues.extend(file_issues)
    largest = sorted(shapes, key=lambda item: (-item.nonblank_lines, item.path))[:largest_count]
    return CheckReport(len(shapes), skipped, tuple(issues), tuple(largest))

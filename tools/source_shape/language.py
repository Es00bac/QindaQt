# SPDX-License-Identifier: GPL-3.0-or-later
"""Find oversized function-shaped blocks without compiler dependencies."""

from __future__ import annotations

import ast
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class FunctionSpan:
    name: str
    start_line: int
    end_line: int

    @property
    def line_count(self) -> int:
        return self.end_line - self.start_line + 1


class SourceParseError(ValueError):
    """A supported source file could not be parsed enough for shape checks."""


def _python_spans(text: str, path: Path) -> list[FunctionSpan]:
    try:
        tree = ast.parse(text, filename=str(path))
    except SyntaxError as error:
        raise SourceParseError(f"Python syntax error at line {error.lineno}: {error.msg}") from error
    spans: list[FunctionSpan] = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.end_lineno is not None:
            spans.append(FunctionSpan(node.name, node.lineno, node.end_lineno))
    return spans


CONTROL_NAMES = {"if", "for", "while", "switch", "catch", "requires"}
FUNCTION_NAME = re.compile(r"([A-Za-z_~][A-Za-z0-9_:~<>]*)\s*\([^(){};]*\)\s*[^;{}]*$")


def _strip_c_like_line(line: str, in_comment: bool) -> tuple[str, bool]:
    result: list[str] = []
    index = 0
    quote: str | None = None
    while index < len(line):
        current = line[index]
        following = line[index + 1] if index + 1 < len(line) else ""
        if in_comment:
            if current == "*" and following == "/":
                in_comment = False
                index += 2
            else:
                index += 1
            continue
        if quote is not None:
            if current == "\\":
                index += 2
            elif current == quote:
                quote = None
                index += 1
            else:
                index += 1
            result.append(" ")
            continue
        if current == "/" and following == "*":
            in_comment = True
            index += 2
        elif current == "/" and following == "/":
            break
        elif current in {'"', "'"}:
            quote = current
            result.append(" ")
            index += 1
        else:
            result.append(current)
            index += 1
    return "".join(result), in_comment


def _candidate_name(header: str) -> str | None:
    normalized = " ".join(header.split())
    match = FUNCTION_NAME.search(normalized)
    if match is None:
        return None
    name = match.group(1).split("::")[-1]
    if name in CONTROL_NAMES or normalized.startswith(("class ", "struct ", "enum ", "namespace ")):
        return None
    return match.group(1)


def _c_like_spans(text: str) -> list[FunctionSpan]:
    spans: list[FunctionSpan] = []
    open_functions: list[tuple[int, int, str]] = []
    depth = 0
    header = ""
    in_comment = False
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line, in_comment = _strip_c_like_line(raw_line, in_comment)
        for character in line:
            if character == "{":
                name = _candidate_name(header)
                depth += 1
                if name is not None:
                    open_functions.append((depth, line_number, name))
                header = ""
            elif character == "}":
                if open_functions and open_functions[-1][0] == depth:
                    _, start_line, name = open_functions.pop()
                    spans.append(FunctionSpan(name, start_line, line_number))
                depth = max(0, depth - 1)
                header = ""
            elif character == ";":
                header = ""
            else:
                header = (header + character)[-800:]
    return spans


def function_spans(path: Path, text: str) -> list[FunctionSpan]:
    if path.suffix == ".py":
        return _python_spans(text, path)
    if path.suffix in {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".qml", ".js"}:
        return _c_like_spans(text)
    return []

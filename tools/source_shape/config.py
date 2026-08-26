# SPDX-License-Identifier: GPL-3.0-or-later
"""Parse the allowlisted, repository-owned source-shape policy."""

from __future__ import annotations

import fnmatch
import json
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any


class ConfigurationError(ValueError):
    """The source-shape policy is malformed or ambiguous."""


@dataclass(frozen=True)
class Limits:
    max_nonblank_lines: int
    review_nonblank_lines: int
    max_bytes: int
    max_function_lines: int
    max_line_characters: int


@dataclass(frozen=True)
class AllowlistRule:
    pattern: str
    reason: str
    skip: bool
    overrides: dict[str, int]


@dataclass(frozen=True)
class ShapeConfig:
    extensions: frozenset[str]
    default_limits: Limits
    extension_limits: dict[str, Limits]
    ignore_patterns: tuple[str, ...]
    allowlist: tuple[AllowlistRule, ...]

    def ignores(self, relative_path: str) -> bool:
        return any(fnmatch.fnmatch(relative_path, pattern) for pattern in self.ignore_patterns)

    def policy_for(self, relative_path: str, extension: str) -> tuple[Limits, str | None, bool]:
        limits = self.extension_limits.get(extension, self.default_limits)
        for rule in self.allowlist:
            if not fnmatch.fnmatch(relative_path, rule.pattern):
                continue
            for key, value in rule.overrides.items():
                limits = replace(limits, **{key: value})
            return limits, rule.reason, rule.skip
        return limits, None, False


LIMIT_KEYS = {
    "max_nonblank_lines",
    "review_nonblank_lines",
    "max_bytes",
    "max_function_lines",
    "max_line_characters",
}


def _mapping(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ConfigurationError(f"{location} must be an object")
    return value


def _positive_int(value: Any, location: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ConfigurationError(f"{location} must be a positive integer")
    return value


def _limits(value: Any, location: str, base: Limits | None = None) -> Limits:
    data = _mapping(value, location)
    unknown = set(data) - LIMIT_KEYS
    if unknown:
        raise ConfigurationError(f"{location} has unknown keys: {sorted(unknown)}")
    if base is None and set(data) != LIMIT_KEYS:
        raise ConfigurationError(f"{location} must define every limit: {sorted(LIMIT_KEYS)}")
    source = {key: getattr(base, key) for key in LIMIT_KEYS} if base else {}
    source.update({key: _positive_int(raw, f"{location}.{key}") for key, raw in data.items()})
    limits = Limits(**source)
    if limits.review_nonblank_lines > limits.max_nonblank_lines:
        raise ConfigurationError(f"{location}.review_nonblank_lines cannot exceed the maximum")
    return limits


def _allowlist(values: Any) -> tuple[AllowlistRule, ...]:
    if not isinstance(values, list):
        raise ConfigurationError("allowlist must be an array")
    rules: list[AllowlistRule] = []
    for index, value in enumerate(values):
        location = f"allowlist[{index}]"
        item = _mapping(value, location)
        pattern = item.get("pattern")
        reason = item.get("reason")
        skip = item.get("skip", False)
        overrides = item.get("limits", {})
        if not isinstance(pattern, str) or not pattern:
            raise ConfigurationError(f"{location}.pattern must be a non-empty string")
        if not isinstance(reason, str) or len(reason.strip()) < 12:
            raise ConfigurationError(f"{location}.reason must explain the exception")
        if not isinstance(skip, bool):
            raise ConfigurationError(f"{location}.skip must be a boolean")
        override_mapping = _mapping(overrides, f"{location}.limits")
        unknown = set(override_mapping) - LIMIT_KEYS
        if unknown:
            raise ConfigurationError(f"{location}.limits has unknown keys: {sorted(unknown)}")
        parsed = {
            key: _positive_int(raw, f"{location}.limits.{key}")
            for key, raw in override_mapping.items()
        }
        if not skip and not parsed:
            raise ConfigurationError(f"{location} must set skip or override at least one limit")
        rules.append(AllowlistRule(pattern, reason.strip(), skip, parsed))
    return tuple(rules)


def load_config(path: Path) -> ShapeConfig:
    try:
        root = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ConfigurationError(f"{path}: {error}") from error
    document = _mapping(root, str(path))
    allowed_root_keys = {
        "version",
        "policy",
        "source_extensions",
        "defaults",
        "extension_limits",
        "ignore_patterns",
        "allowlist",
    }
    unknown_root_keys = set(document) - allowed_root_keys
    if unknown_root_keys:
        raise ConfigurationError(f"source-shape config has unknown keys: {sorted(unknown_root_keys)}")
    if document.get("version") != 1:
        raise ConfigurationError("source-shape config version must be 1")
    if not isinstance(document.get("policy"), str) or not document["policy"].strip():
        raise ConfigurationError("source-shape config policy must be a non-empty string")
    raw_extensions = document.get("source_extensions")
    if not isinstance(raw_extensions, list) or not raw_extensions:
        raise ConfigurationError("source_extensions must be a non-empty array")
    extensions = frozenset(raw_extensions)
    if any(not isinstance(item, str) or not item.startswith(".") for item in extensions):
        raise ConfigurationError("every source extension must start with '.'")
    defaults = _limits(document.get("defaults"), "defaults")
    raw_extension_limits = _mapping(document.get("extension_limits", {}), "extension_limits")
    extension_limits = {
        extension: _limits(value, f"extension_limits.{extension}", defaults)
        for extension, value in raw_extension_limits.items()
    }
    if not set(extension_limits).issubset(extensions):
        unknown = sorted(set(extension_limits) - extensions)
        raise ConfigurationError(f"extension_limits contains untracked extensions: {unknown}")
    raw_ignores = document.get("ignore_patterns", [])
    if not isinstance(raw_ignores, list) or any(not isinstance(item, str) for item in raw_ignores):
        raise ConfigurationError("ignore_patterns must be an array of strings")
    return ShapeConfig(
        extensions=extensions,
        default_limits=defaults,
        extension_limits=extension_limits,
        ignore_patterns=tuple(raw_ignores),
        allowlist=_allowlist(document.get("allowlist", [])),
    )

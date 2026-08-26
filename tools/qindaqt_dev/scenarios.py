# SPDX-License-Identifier: GPL-3.0-or-later
"""Load and validate declarative virtual-display scenarios."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


ID_PATTERN = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
OUTPUT_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9._-]*$")
TRANSFORMS = {
    "normal",
    "rotate-90",
    "rotate-180",
    "rotate-270",
    "flipped",
    "flipped-90",
    "flipped-180",
    "flipped-270",
}

# AGENT-CONTRACT: This validator and tests/scenarios/schema.json jointly define
# scenario v1. Keep both synchronized; bootstrap CI intentionally has no jsonschema
# dependency, so the Python checks cannot be replaced by schema-only validation.
EVENT_ACTIONS = {
    "enable",
    "disable",
    "move",
    "set-mode",
    "set-primary",
    "set-scale",
    "set-transform",
}


class ScenarioError(ValueError):
    """A scenario cannot be used because its data violates the public schema."""


@dataclass(frozen=True)
class Output:
    """Validated initial state for one virtual output."""

    name: str
    enabled: bool
    primary: bool
    x: int
    y: int
    width: int
    height: int
    refresh_hz: float
    scale: float
    transform: str

    @property
    def transformed_size(self) -> tuple[int, int]:
        if self.transform in {"rotate-90", "rotate-270", "flipped-90", "flipped-270"}:
            return self.height, self.width
        return self.width, self.height


@dataclass(frozen=True)
class Scenario:
    """A validated scenario plus its source path and original JSON document."""

    scenario_id: str
    description: str
    profile: str
    theme: str
    outputs: tuple[Output, ...]
    events: tuple[dict[str, Any], ...]
    path: Path
    document: dict[str, Any]

    @property
    def enabled_outputs(self) -> tuple[Output, ...]:
        return tuple(output for output in self.outputs if output.enabled)

    @property
    def canvas_size(self) -> tuple[int, int]:
        """Return the smallest canvas enclosing all initially enabled outputs."""
        enabled = self.enabled_outputs
        minimum_x = min(output.x for output in enabled)
        minimum_y = min(output.y for output in enabled)
        maximum_x = max(output.x + output.transformed_size[0] for output in enabled)
        maximum_y = max(output.y + output.transformed_size[1] for output in enabled)
        return maximum_x - minimum_x, maximum_y - minimum_y


class ScenarioRepository:
    """Discovers scenario documents while excluding the JSON schema itself."""

    def __init__(self, directory: Path) -> None:
        self.directory = directory.resolve()

    def paths(self) -> list[Path]:
        if not self.directory.is_dir():
            raise ScenarioError(f"scenario directory does not exist: {self.directory}")
        return sorted(path for path in self.directory.glob("*.json") if path.name != "schema.json")

    def load(self, scenario_id: str) -> Scenario:
        if not ID_PATTERN.fullmatch(scenario_id):
            raise ScenarioError(f"invalid scenario id: {scenario_id!r}")
        path = self.directory / f"{scenario_id}.json"
        if not path.is_file():
            available = ", ".join(item.stem for item in self.paths()) or "none"
            raise ScenarioError(f"unknown scenario {scenario_id!r}; available: {available}")
        return load_scenario(path)

    def load_all(self) -> list[Scenario]:
        paths = self.paths()
        if not paths:
            raise ScenarioError(f"no scenario documents found in {self.directory}")
        return [load_scenario(path) for path in paths]


def _expect_mapping(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ScenarioError(f"{location} must be an object")
    return value


def _reject_unknown(item: dict[str, Any], allowed: set[str], location: str) -> None:
    unknown = set(item) - allowed
    if unknown:
        raise ScenarioError(f"{location} has unknown keys: {sorted(unknown)}")


def _expect_bool(value: Any, location: str) -> bool:
    if not isinstance(value, bool):
        raise ScenarioError(f"{location} must be a boolean")
    return value


def _expect_number(value: Any, location: str, *, minimum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value < minimum:
        raise ScenarioError(f"{location} must be a number >= {minimum}")
    return float(value)


def _expect_int(value: Any, location: str, *, minimum: int | None = None) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ScenarioError(f"{location} must be an integer")
    if minimum is not None and value < minimum:
        raise ScenarioError(f"{location} must be >= {minimum}")
    return value


def _expect_string(value: Any, location: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ScenarioError(f"{location} must be a non-empty string")
    return value


def _validate_output(value: Any, index: int) -> Output:
    location = f"outputs[{index}]"
    item = _expect_mapping(value, location)
    _reject_unknown(item, {"name", "enabled", "primary", "position", "mode", "scale", "transform"}, location)
    name = _expect_string(item.get("name"), f"{location}.name")
    if not OUTPUT_PATTERN.fullmatch(name):
        raise ScenarioError(f"{location}.name contains unsupported characters: {name!r}")
    position = _expect_mapping(item.get("position"), f"{location}.position")
    mode = _expect_mapping(item.get("mode"), f"{location}.mode")
    _reject_unknown(position, {"x", "y"}, f"{location}.position")
    _reject_unknown(mode, {"width", "height", "refresh_hz"}, f"{location}.mode")
    transform = _expect_string(item.get("transform"), f"{location}.transform")
    if transform not in TRANSFORMS:
        raise ScenarioError(f"{location}.transform must be one of {sorted(TRANSFORMS)}")
    return Output(
        name=name,
        enabled=_expect_bool(item.get("enabled"), f"{location}.enabled"),
        primary=_expect_bool(item.get("primary"), f"{location}.primary"),
        x=_expect_int(position.get("x"), f"{location}.position.x"),
        y=_expect_int(position.get("y"), f"{location}.position.y"),
        width=_expect_int(mode.get("width"), f"{location}.mode.width", minimum=320),
        height=_expect_int(mode.get("height"), f"{location}.mode.height", minimum=200),
        refresh_hz=_expect_number(mode.get("refresh_hz"), f"{location}.mode.refresh_hz", minimum=1),
        scale=_expect_number(item.get("scale"), f"{location}.scale", minimum=0.5),
        transform=transform,
    )


def _validate_event_value(event: dict[str, Any], location: str, action: str) -> None:
    if action in {"enable", "disable", "set-primary"}:
        if "value" in event:
            raise ScenarioError(f"{location}.value is not valid for action {action!r}")
        return
    if "value" not in event:
        raise ScenarioError(f"{location}.value is required for action {action!r}")
    value = event["value"]
    if action == "move":
        position = _expect_mapping(value, f"{location}.value")
        _reject_unknown(position, {"x", "y"}, f"{location}.value")
        _expect_int(position.get("x"), f"{location}.value.x")
        _expect_int(position.get("y"), f"{location}.value.y")
    elif action == "set-mode":
        mode = _expect_mapping(value, f"{location}.value")
        _reject_unknown(mode, {"width", "height", "refresh_hz"}, f"{location}.value")
        _expect_int(mode.get("width"), f"{location}.value.width", minimum=320)
        _expect_int(mode.get("height"), f"{location}.value.height", minimum=200)
        _expect_number(mode.get("refresh_hz"), f"{location}.value.refresh_hz", minimum=1)
    elif action == "set-scale":
        _expect_number(value, f"{location}.value", minimum=0.5)
    elif action == "set-transform":
        transform = _expect_string(value, f"{location}.value")
        if transform not in TRANSFORMS:
            raise ScenarioError(f"{location}.value must be one of {sorted(TRANSFORMS)}")


def _validate_events(values: Any, outputs: tuple[Output, ...]) -> tuple[dict[str, Any], ...]:
    if not isinstance(values, list):
        raise ScenarioError("events must be an array")
    validated: list[dict[str, Any]] = []
    output_names = {output.name for output in outputs}
    enabled = {output.name: output.enabled for output in outputs}
    primary = next(output.name for output in outputs if output.enabled and output.primary)
    previous_time = -1
    for index, raw_event in enumerate(values):
        location = f"events[{index}]"
        event = _expect_mapping(raw_event, location)
        _reject_unknown(event, {"at_ms", "action", "output", "value"}, location)
        at_ms = _expect_int(event.get("at_ms"), f"{location}.at_ms", minimum=0)
        action = _expect_string(event.get("action"), f"{location}.action")
        output = _expect_string(event.get("output"), f"{location}.output")
        if at_ms < previous_time:
            raise ScenarioError("events must be ordered by at_ms")
        if action not in EVENT_ACTIONS:
            raise ScenarioError(f"{location}.action must be one of {sorted(EVENT_ACTIONS)}")
        if output not in output_names:
            raise ScenarioError(f"{location}.output references unknown output {output!r}")
        _validate_event_value(event, location, action)
        if action == "enable":
            enabled[output] = True
        elif action == "set-primary":
            if not enabled[output]:
                raise ScenarioError(f"{location} cannot make a disabled output primary")
            primary = output
        elif action == "disable":
            if output == primary:
                raise ScenarioError(f"{location} must transfer primary before disabling {output!r}")
            enabled[output] = False
        # AGENT-GUARD: Every event boundary must retain an enabled primary output;
        # compositor automation consumes events one by one and observes each boundary.
        if not enabled.get(primary, False):
            raise ScenarioError(f"{location} leaves the topology without an enabled primary output")
        previous_time = at_ms
        validated.append(dict(event))
    return tuple(validated)


def load_scenario(path: Path) -> Scenario:
    """Load one JSON document, returning all validation failures as ScenarioError."""
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ScenarioError(f"{path}: {error}") from error
    root = _expect_mapping(document, str(path))
    _reject_unknown(
        root,
        {"$schema", "schema_version", "id", "description", "profile", "theme", "outputs", "events"},
        str(path),
    )
    version = _expect_int(root.get("schema_version"), "schema_version", minimum=1)
    if version != 1:
        raise ScenarioError(f"unsupported schema_version {version}; this harness supports 1")
    scenario_id = _expect_string(root.get("id"), "id")
    if not ID_PATTERN.fullmatch(scenario_id):
        raise ScenarioError(f"id must be a lowercase kebab-case identifier: {scenario_id!r}")
    if scenario_id != path.stem:
        raise ScenarioError(f"id {scenario_id!r} must match filename {path.stem!r}")
    raw_outputs = root.get("outputs")
    if not isinstance(raw_outputs, list) or not raw_outputs:
        raise ScenarioError("outputs must be a non-empty array")
    outputs = tuple(_validate_output(value, index) for index, value in enumerate(raw_outputs))
    names = [output.name for output in outputs]
    if len(names) != len(set(names)):
        raise ScenarioError("output names must be unique")
    enabled = [output for output in outputs if output.enabled]
    if not enabled:
        raise ScenarioError("at least one output must initially be enabled")
    primaries = [output for output in enabled if output.primary]
    if len(primaries) != 1:
        raise ScenarioError("exactly one initially enabled output must be primary")
    if any(output.primary and not output.enabled for output in outputs):
        raise ScenarioError("a disabled output cannot initially be primary")
    events = _validate_events(root.get("events", []), outputs)
    return Scenario(
        scenario_id=scenario_id,
        description=_expect_string(root.get("description"), "description"),
        profile=_expect_string(root.get("profile"), "profile"),
        theme=_expect_string(root.get("theme"), "theme"),
        outputs=outputs,
        events=events,
        path=path.resolve(),
        document=root,
    )


def scenario_summaries(scenarios: Iterable[Scenario]) -> list[dict[str, Any]]:
    """Produce stable, machine-readable rows used by list output and CI fixtures."""
    return [
        {
            "id": scenario.scenario_id,
            "description": scenario.description,
            "canvas": f"{scenario.canvas_size[0]}x{scenario.canvas_size[1]}",
            "enabled_outputs": len(scenario.enabled_outputs),
            "events": len(scenario.events),
        }
        for scenario in scenarios
    ]

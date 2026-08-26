# SPDX-License-Identifier: GPL-3.0-or-later
"""Command-line interface for isolated QindaQt development sessions."""

from __future__ import annotations

import argparse
import json
import shlex
import sys
from pathlib import Path
from typing import Sequence

from .backends import BACKEND_NAMES, build_plan, plan_as_dict
from .isolation import IsolatedRuntime, environment_preview, execute_plan
from .scenarios import ScenarioError, ScenarioRepository, scenario_summaries


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SCENARIO_DIRECTORY = REPOSITORY_ROOT / "tests" / "scenarios"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="qindaqt-dev-session",
        description="Validate scenarios and plan or run an isolated QindaQt development session.",
    )
    parser.add_argument(
        "--scenario-dir",
        type=Path,
        default=DEFAULT_SCENARIO_DIRECTORY,
        help="scenario directory (default: repository tests/scenarios)",
    )
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--list-scenarios", action="store_true", help="list all validated scenarios")
    actions.add_argument("--validate-scenarios", action="store_true", help="validate every scenario")
    actions.add_argument("--list-backends", action="store_true", help="list supported backend names")
    actions.add_argument("--scenario", metavar="ID", help="plan or run one scenario")
    parser.add_argument("--backend", choices=BACKEND_NAMES, default="preview")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="print the launch plan without creating files")
    mode.add_argument("--execute", action="store_true", help="execute the plan in disposable XDG directories")
    parser.add_argument("--program", help="override the backend executable (useful for harness tests)")
    parser.add_argument(
        "--smoke-test",
        action="store_true",
        help="make preview validate its catalogs and exit instead of opening a window",
    )
    parser.add_argument("--timeout", type=float, help="terminate an executed foreground process after N seconds")
    parser.add_argument("--json", action="store_true", help="emit stable JSON output")
    return parser


def _print_scenarios(repository: ScenarioRepository, as_json: bool) -> int:
    rows = scenario_summaries(repository.load_all())
    if as_json:
        print(json.dumps(rows, indent=2, sort_keys=True))
        return 0
    for row in rows:
        print(
            f"{row['id']:<28} {row['canvas']:>11}  "
            f"outputs={row['enabled_outputs']} events={row['events']}  {row['description']}"
        )
    return 0


def _validate(repository: ScenarioRepository, as_json: bool) -> int:
    scenarios = repository.load_all()
    result = {"valid": True, "count": len(scenarios), "directory": str(repository.directory)}
    if as_json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"Validated {len(scenarios)} scenarios in {repository.directory}")
    return 0


def _print_plan(repository: ScenarioRepository, arguments: argparse.Namespace) -> int:
    scenario = repository.load(arguments.scenario)
    plan = build_plan(
        scenario,
        arguments.backend,
        program=arguments.program,
        smoke_test=arguments.smoke_test,
    )
    if arguments.execute:
        with IsolatedRuntime() as runtime:
            payload = plan_as_dict(plan, runtime.root)
            payload["environment_overrides"] = dict(sorted(environment_preview(plan).items()))
            if arguments.json:
                print(json.dumps(payload, indent=2, sort_keys=True))
            else:
                print(f"Launching {scenario.scenario_id!r} in isolated runtime {runtime.root}")
                print(f"Command: {shlex.join(plan.argv)}")
            return execute_plan(plan, runtime, arguments.timeout)
    # AGENT-GUARD: Dry-run is deliberately the default. This makes copied commands safe when the
    # user omitted --dry-run and requires explicit --execute for process creation.
    payload = plan_as_dict(plan, "<temporary-xdg-root>")
    payload["environment_overrides"] = dict(sorted(environment_preview(plan).items()))
    if arguments.json:
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        print(f"Dry-run for scenario {scenario.scenario_id!r} using backend {plan.backend!r}")
        print(f"Command: {shlex.join(plan.argv)}")
        print("Isolation: HOME and all XDG state use <temporary-xdg-root>; a private D-Bus is preferred.")
        for note in plan.notes:
            print(f"Note: {note}")
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    parser = _parser()
    arguments = parser.parse_args(argv)
    if arguments.timeout is not None and arguments.timeout <= 0:
        parser.error("--timeout must be greater than zero")
    if not any(
        (
            arguments.list_scenarios,
            arguments.validate_scenarios,
            arguments.list_backends,
            arguments.scenario,
        )
    ):
        parser.error("choose --list-scenarios, --validate-scenarios, --list-backends, or --scenario")
    if not arguments.scenario and (arguments.dry_run or arguments.execute or arguments.program):
        parser.error("--dry-run, --execute, and --program require --scenario")
    if arguments.smoke_test and (not arguments.scenario or arguments.backend != "preview"):
        parser.error("--smoke-test requires a preview --scenario")
    repository = ScenarioRepository(arguments.scenario_dir)
    try:
        if arguments.list_scenarios:
            return _print_scenarios(repository, arguments.json)
        if arguments.validate_scenarios:
            return _validate(repository, arguments.json)
        if arguments.list_backends:
            print(json.dumps(BACKEND_NAMES) if arguments.json else "\n".join(BACKEND_NAMES))
            return 0
        return _print_plan(repository, arguments)
    except (FileNotFoundError, ScenarioError, ValueError) as error:
        print(f"qindaqt-dev-session: error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

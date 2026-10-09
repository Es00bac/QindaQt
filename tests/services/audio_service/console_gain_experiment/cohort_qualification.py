# SPDX-License-Identifier: GPL-3.0-or-later
"""Strict old-only diagnostic evidence admission; no package or runtime action."""
from __future__ import annotations
from pathlib import Path
from evidence_io import exact_record, json_value
from upstream_suite import inspect_suite

MODES = {"unpatched-asan-control", "patched-asan", "patched-normal"}
QUALIFICATION_KEYS = {"diagnosticOnly", "upstreamTests", "upstreamEvidence"}
EVIDENCE_KEYS = {"schema", "mode", "root", "recipePath", "compileExit", "testExit",
                 "suiteComplete", "testEnvironmentVerified", "registryCount", "counts", "raw"}
BASE_RAW = {"compileLog", "phases", "inputsFinal"}
SUITE_RAW = {"registry", "json", "text", "junit", "testLog"}
COMPILE_PHASES = ["clean", "setup", "unpack", "prepare", "configure", "compile"]


def private_bus_values(root: Path) -> dict[str, str]:
    return {"DBUS_SESSION_BUS_ADDRESS": f"unix:path={root}/absent-session",
            "DBUS_SYSTEM_BUS_ADDRESS": f"unix:path={root}/absent-system"}


def preserved_env_unset(value: str) -> str:
    # Preserve only two locally constructed values through setup and the saved
    # EAPI environment. Every other configured unset remains in force.
    if not isinstance(value, str) or "\0" in value or len(value) > 65536:
        raise ValueError("invalid ENV_UNSET baseline")
    keys=private_bus_values(Path("/fixed"))
    # ENV_UNSET is incremental: mere omission does not remove profile entries.
    # Explicit negatives preserve every other configured unset while overriding
    # exactly these two locally supplied absent sockets, never a host address.
    retained=[key for key in value.split() if key not in keys and key not in
              {"-"+name for name in keys}]
    return " ".join([*retained,*("-"+name for name in keys)])


def admit_qualification(value: dict, mode: str, *, allow_failed_old: bool = False) -> dict:
    if (not isinstance(mode, str) or mode not in MODES
            or type(allow_failed_old) is not bool or not isinstance(value, dict) or set(value) != QUALIFICATION_KEYS
            or type(value["diagnosticOnly"]) is not bool
            or not isinstance(value["upstreamTests"], str) or value["upstreamTests"] not in {"passed", "failed", "not-requested"}):
        raise ValueError("unknown qualification flags")
    if allow_failed_old and mode != "unpatched-asan-control":
        raise ValueError("failed-suite opt-in is old-ASan-only")
    status = value["upstreamTests"]
    diagnostic = value["diagnosticOnly"]
    if diagnostic != (status == "failed"):
        raise ValueError("contradictory diagnostic qualification")
    if diagnostic and (mode != "unpatched-asan-control" or not allow_failed_old):
        raise ValueError("failed upstream suite is not a release or fixed qualification")
    if status == "not-requested" and mode != "patched-normal":
        raise ValueError("ASan cohorts require the complete upstream suite")
    evidence = value["upstreamEvidence"]
    if (not isinstance(evidence, dict) or set(evidence) != EVIDENCE_KEYS
            or type(evidence["schema"]) is not int or evidence["schema"] != 1
            or evidence["mode"] != mode or evidence["compileExit"] != 0
            or type(evidence["compileExit"]) is not int
            or type(evidence["suiteComplete"]) is not bool
            or type(evidence["testEnvironmentVerified"]) is not bool):
        raise ValueError("malformed qualification evidence")
    if (not isinstance(evidence["root"], str) or not 0 < len(evidence["root"]) <= 4096
            or not isinstance(evidence["recipePath"], str)
            or not 0 < len(evidence["recipePath"]) <= 4096):
        raise ValueError("invalid evidence path")
    root = Path(evidence["root"])
    recipe = Path(evidence["recipePath"])
    if (not root.is_absolute() or root == Path("/") or str(root.resolve(strict=True)) != str(root)
            or not recipe.is_absolute() or str(recipe.resolve(strict=True)) != str(recipe)):
        raise ValueError("qualification paths are not canonical")
    raw = evidence["raw"]
    required = BASE_RAW | (SUITE_RAW if status != "not-requested" else set())
    if not isinstance(raw, dict) or set(raw) != required:
        raise ValueError("unknown/missing qualification payloads")
    payloads = {}
    for name, record in raw.items():
        payloads[name] = exact_record(record)
        if not Path(record["path"]).is_relative_to(root / "suite-evidence"):
            raise ValueError("qualification payload escaped its private snapshot")
    inputs = json_value(payloads["inputsFinal"])
    if not isinstance(inputs, dict) or inputs.get("verified") is not True:
        raise ValueError("failed final source-input observation")
    ledger = json_value(payloads["phases"])
    test_variant = status != "not-requested"
    if (not isinstance(ledger, dict) or ledger.get("mode") != mode
            or ledger.get("upstreamTestVariant") is not test_variant
            or not isinstance(ledger.get("steps"), list)
            or len(ledger["steps"]) != (2 if test_variant else 1)):
        raise ValueError("phase state is incomplete or contradictory")
    for step, name, phases, log in zip(ledger["steps"],
        ["portage-compile", "portage-test"], [COMPILE_PHASES, ["test"]],
        ["compileLog", "testLog"]):
        expected_exit = 0 if name == "portage-compile" or status == "passed" else 1
        validate_phase(step, recipe, phases, name, {expected_exit}, raw[log]["sha256"])
    counts = evidence["counts"]
    if (not isinstance(counts, dict) or set(counts) != {"ok", "fail", "timeout"}
            or any(type(n) is not int or n < 0 for n in counts.values())
            or type(evidence["registryCount"]) is not int):
        raise ValueError("malformed suite count")
    if test_variant:
        observed = inspect_suite(payloads, private_bus_values(root))
        if (observed["upstreamTests"] != status or observed["counts"] != counts
                or observed["registryCount"] != evidence["registryCount"]
                or evidence["testExit"] != (1 if diagnostic else 0)
                or type(evidence["testExit"]) is not int
                or evidence["suiteComplete"] is not True
                or evidence["testEnvironmentVerified"] is not True):
            raise ValueError("claimed suite contradicts actual full-suite evidence")
    elif (counts != {"ok": 0, "fail": 0, "timeout": 0}
          or evidence["registryCount"] != 0 or evidence["testExit"] is not None
          or evidence["suiteComplete"] is not False
          or evidence["testEnvironmentVerified"] is not False):
        raise ValueError("unrequested suite cannot claim test qualification")
    return value




def admit_publication_inputs(value: dict, qualification: dict) -> None:
    raw = exact_record(value)
    root = Path(qualification["upstreamEvidence"]["root"])
    if not Path(value["path"]).is_relative_to(root / "suite-evidence"):
        raise ValueError("publication input proof escaped its qualification snapshot")
    observed = json_value(raw)
    if not isinstance(observed, dict) or observed.get("verified") is not True:
        raise ValueError("publication source-input observation failed")

def validate_phase(step: dict, recipe: Path, phases: list[str], name: str,
                   allowed_exits: set[int], checksum: str | None = None) -> None:
    if (not isinstance(step, dict) or step.get("name") != name
            or step.get("argv") != ["/usr/bin/ebuild", str(recipe), *phases]
            or type(step.get("exit")) is not int or step["exit"] not in allowed_exits
            or step.get("timedOut") is not False
            or step.get("leaderReservedThroughCleanup") is not True
            or type(step.get("ownedProcessGroup")) is not int or step["ownedProcessGroup"] <= 0
            or checksum is not None and step.get("sha256") != checksum):
        raise ValueError("phase failed, timed out, repeated or cleanup uncertain")

def image_features(features: str, mode: str, qualification: dict) -> str:
    # Only the already admitted OLD failed-suite transition can bypass
    # dependency recursion. No tested marker is created; USE/test stays set.
    if (not isinstance(mode, str) or mode not in MODES or not isinstance(qualification, dict)
            or set(qualification) != QUALIFICATION_KEYS
            or type(qualification.get("diagnosticOnly")) is not bool):
        raise ValueError("unknown diagnostic phase mode")
    words = features.split()
    if not words or words[0] != "-*" or "noauto" in words:
        raise ValueError("invalid phase feature state")
    if qualification["diagnosticOnly"]:
        if mode != "unpatched-asan-control" or qualification["upstreamTests"] != "failed":
            raise ValueError("invalid diagnostic image phase")
        if "test" not in words or "test-fail-continue" in words:
            raise ValueError("diagnostic image retains strict test policy")
        return features + " noauto"
    return features

# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit compile, single test observation and private-image phase admission."""
from __future__ import annotations
import json
import os
from pathlib import Path
import stat

from cohort_qualification import (COMPILE_PHASES, MODES, admit_qualification,
                                  image_features, private_bus_values, validate_phase)
from evidence_io import snapshot
from upstream_suite import inspect_suite
from portage_environment import admit_saved_environment


def _marker(path: Path) -> bool:
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
    except FileNotFoundError:
        return False
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_size != 0:
            raise ValueError("invalid observed Portage phase marker")
        return True
    finally:
        os.close(descriptor)


def before_image(build: Path, qualification: dict) -> None:
    if not _marker(build / ".compiled") or any(
            _marker(build / name) for name in (".installed", ".packaged")):
        raise ValueError("compile/image phase state is not fresh")
    tested = _marker(build / ".tested")
    if tested != (qualification["upstreamTests"] == "passed"):
        raise ValueError("tested marker contradicts observed upstream result")


def compile_and_observe(recipe: Path, environment: dict[str, str], output: Path,
                        build: Path, inputs, record, mode: str, test_variant: bool,
                        allow_failed_old: bool, attempt) -> dict:
    """Borrowed synchronous collaborators must not dispatch unowned processes.

    Exceptions stop assembly. A returned failed test is admitted only after
    complete actual producer evidence; no retry or marker write exists here.
    """
    if (not isinstance(mode, str) or mode not in MODES or type(allow_failed_old) is not bool
            or type(test_variant) is not bool
            or allow_failed_old and mode != "unpatched-asan-control"
            or mode != "patched-normal" and not test_variant):
        raise ValueError("invalid explicit diagnostic mode")
    compile_step = attempt(recipe, COMPILE_PHASES, environment, output, inputs,
                           record, name="portage-compile")
    validate_phase(compile_step, recipe, COMPILE_PHASES, "portage-compile", {0})
    if not _marker(build / ".compiled") or _marker(build / ".tested"):
        raise ValueError("compile phase state is incomplete or already tested")
    evidence = output / "suite-evidence"
    evidence.mkdir(mode=0o700)
    raw = {"compileLog": snapshot(Path(compile_step["log"]), evidence / "compile.log")}
    validate_phase(compile_step, recipe, COMPILE_PHASES, "portage-compile", {0},
                   raw["compileLog"]["sha256"])
    if test_variant:
        work = build / "work/pipewire-1.6.8-abi_x86_64.amd64"
        # Freeze the compiled registry BEFORE executing the suite. Missing or
        # changed observations cannot shrink the required set after a failure.
        raw["registry"] = snapshot(work / "meson-info/intro-tests.json", evidence / "registry.json")
        saved=admit_saved_environment(build / "temp/environment",environment)
        (output / "saved-private-bus-before-test.json").write_text(
            json.dumps(saved,sort_keys=True,indent=2)+"\n")
        test_step = attempt(recipe, ["test"], environment, output, inputs,
                            record, name="portage-test")
        validate_phase(test_step, recipe, ["test"], "portage-test", {0, 1})
        saved=admit_saved_environment(build / "temp/environment",environment)
        (output / "saved-private-bus-after-test.json").write_text(
            json.dumps(saved,sort_keys=True,indent=2)+"\n")
        raw["testLog"] = snapshot(Path(test_step["log"]), evidence / "test-phase.log")
        for key, filename in (("json", "testlog.json"), ("text", "testlog.txt"),
                              ("junit", "testlog.junit.xml")):
            raw[key] = snapshot(work / "meson-logs" / filename, evidence / filename)
        from evidence_io import exact_record
        observed = inspect_suite({key: exact_record(raw[key]) for key in
                                  ("registry", "json", "text", "junit")}, private_bus_values(output))
        status = observed["upstreamTests"]
        count = observed["registryCount"]
        counts = observed["counts"]
        exitcode = test_step["exit"]
    else:
        status, count, counts, exitcode = "not-requested", 0, {"ok": 0, "fail": 0, "timeout": 0}, None
    raw["phases"] = snapshot(output / "results.json", evidence / "phase-results.json")
    raw["inputsFinal"] = snapshot(output / "input-final-verification.json", evidence / "test-inputs.json")
    qualification = {"diagnosticOnly": status == "failed", "upstreamTests": status,
        "upstreamEvidence": {"schema": 1, "mode": mode, "root": str(output),
            "recipePath": str(recipe), "compileExit": 0, "testExit": exitcode,
            "suiteComplete": test_variant, "testEnvironmentVerified": test_variant,
            "registryCount": count, "counts": counts, "raw": raw}}
    admit_qualification(qualification, mode, allow_failed_old=allow_failed_old)
    before_image(build, qualification)
    (output / "upstream-qualification.json").write_text(
        json.dumps(qualification, sort_keys=True, indent=2) + "\n")
    return qualification


def assemble_image(recipe: Path, environment: dict[str, str], output: Path,
                   build: Path, inputs, record, mode: str, qualification: dict,
                   allow_failed_old: bool, attempt) -> dict:
    admit_qualification(qualification, mode, allow_failed_old=allow_failed_old)
    before_image(build, qualification)
    selected = {**environment, "FEATURES": image_features(
        environment["FEATURES"], mode, qualification)}
    step = attempt(recipe, ["install", "package"], selected, output, inputs,
                   record, name="portage-image")
    validate_phase(step, recipe, ["install", "package"], "portage-image", {0, 1})
    # A failed original suite stays failed even after image construction. Never
    # fabricate .tested, accept a dependency rerun, or turn this into release.
    if qualification["diagnosticOnly"] and _marker(build / ".tested"):
        raise ValueError("failed suite was retried or its marker changed")
    return step

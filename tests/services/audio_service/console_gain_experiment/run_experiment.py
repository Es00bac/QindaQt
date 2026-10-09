#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/run a receipt-pinned private cohort only after the manager grants it."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import xml.etree.ElementTree as ET

from artifact_receipt import artifact, bootstrap_environment, ORIGINAL_RECIPE, PATCHED_RECIPE
from process_evidence import run_logged
from cohort_qualification import admit_qualification, admit_publication_inputs, QUALIFICATION_KEYS
from evidence_io import exact_record, json_value, read_evidence

ROWS = ("routing_gain", "send_admission", "private_environment", "image_preflight", "qualification_receipt",
        "core_scope_runtime", "loopback_gain_runtime", "borrowed_core_lifetime")


def validate_receipt(path: Path, *, allow_failed_old: bool = False) -> tuple[dict, bytes]:
    data = read_evidence(path, readonly=True)
    if not 0 < len(data) <= 65536:
        raise ValueError("receipt outside bounded contract")
    receipt = json_value(data)
    if not isinstance(receipt, dict):
        raise ValueError("cohort receipt must be an object")
    mode = receipt.get("mode")
    keys = {"schema", "mode", "imageRoot", "packageVerification", "package",
        "overlayCandidate", "archiveSha256", "moduleSourceSha256", "recipeSha256",
        "patchSha256", "moduleDirectory", "spaDirectory", "libraryDirectory",
        "artifacts", "scope", "finalInputVerification"} | QUALIFICATION_KEYS
    if set(receipt) != keys or type(receipt.get("schema")) is not int or receipt.get("schema") != 2 or mode not in {
        "patched-normal", "patched-asan", "unpatched-asan-control"
    }:
        raise ValueError("unknown cohort")
    admit_qualification({key: receipt[key] for key in QUALIFICATION_KEYS}, mode,
                        allow_failed_old=allow_failed_old)
    admit_publication_inputs(receipt["finalInputVerification"], receipt)
    old = mode == "unpatched-asan-control"
    if receipt.get("recipeSha256") != (ORIGINAL_RECIPE if old else PATCHED_RECIPE):
        raise ValueError("recipe cohort changed")
    for record in receipt["artifacts"].values():
        if artifact(Path(record["path"])) != record:
            raise ValueError("artifact changed after receipt")
    return receipt, data


def qt_totals(path: Path) -> dict:
    root = ET.parse(path).getroot()
    totals = {"pass": 0, "fail": 0, "skip": 0, "blacklisted": 0}
    for incident in list(root.iter("Incident")) + list(root.iter("Message")):
        kind = incident.get("type", "")
        if kind == "pass" and incident.tag == "Incident": totals["pass"] += 1
        elif kind in {"fail", "xpass"}: totals["fail"] += 1
        elif kind in {"skip", "xfail"}: totals["skip"] += 1
        elif kind.startswith("b"): totals["blacklisted"] += 1
    return totals


def qt_passed(step: dict) -> bool:
    totals = step.get("qt", {})
    return (step["exit"] == 0 and not step["timedOut"]
            and not step.get("xmlIncomplete") and totals.get("pass", 0) >= 3
            and totals.get("fail", 0) == totals.get("skip", 0) == totals.get("blacklisted", 0) == 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--cohort-receipt", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=24)
    parser.add_argument("--load", type=int, default=24)
    parser.add_argument("--allow-failed-upstream-old-asan", action="store_true",
                        help="admit only the explicit original failed-suite diagnostic image")
    args = parser.parse_args()
    if args.jobs < 1 or args.load < 1:
        parser.error("jobs/load must be positive")
    # Refuse loader overrides before any child exec; native bootstrap is an
    # explicit environment, not an attempt to undo an already loaded image.
    if any(key.startswith("LD_") or key == "PIPEWIRE_MODULE_DIR" for key in os.environ):
        parser.error("remove ambient dynamic-loader/module overrides before invoking")
    recipe = Path(__file__).resolve().parent
    source = recipe.parents[3]
    build = args.build.resolve()
    if build == source or source in build.parents and ".cache" not in build.parts:
        parser.error("use an ignored .cache tree or an external temporary build")
    if build.exists() and any(build.iterdir()):
        parser.error("use a fresh build/evidence directory for each immutable cohort")
    receipt, receipt_bytes = validate_receipt(args.cohort_receipt.resolve(strict=True),
        allow_failed_old=args.allow_failed_upstream_old_asan)
    build.mkdir(parents=True, mode=0o700, exist_ok=True)
    evidence = build / "experiment-evidence"; evidence.mkdir(mode=0o700)
    pinned = evidence / "cohort.json"; pinned.write_bytes(receipt_bytes); pinned.chmod(0o600)
    pin = hashlib.sha256(receipt_bytes).hexdigest()
    mode = receipt["mode"]; old = mode == "unpatched-asan-control"
    asan = mode != "patched-normal"
    # Compiler/bootstrap steps use no native loader overrides at all.
    compiler_env = {"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8", "LC_ALL": "C.UTF-8",
                    "CCACHE_DIR": str(evidence / "ccache"),
                    "DBUS_SESSION_BUS_ADDRESS": f"unix:path={evidence}/absent-session",
                    "DBUS_SYSTEM_BUS_ADDRESS": f"unix:path={evidence}/absent-system"}
    configure = ["/usr/bin/cmake", "-S", str(recipe), "-B", str(build),
                 "-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                 "-DQINDAQT_GAIN_COHORT_RECEIPT=" + str(pinned),
                 "-DQINDAQT_GAIN_COHORT_RECEIPT_SHA256=" + pin,
                 "-DQINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL=" + ("ON" if old else "OFF"),
                 "-DQINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN="
                     + ("ON" if args.allow_failed_upstream_old_asan else "OFF")]
    if asan:
        flags = "-fsanitize=address -fno-omit-frame-pointer -g3"
        configure += ["-DCMAKE_CXX_FLAGS=" + flags,
                      "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"]
    results = []
    def record(step: dict) -> None:
        results.append(step)
        (evidence / "results.json").write_text(json.dumps({
            "scope": "private diagnostic; production gain, signed/installed/hardware remain pending",
            "mode": mode, "diagnosticOnly": receipt["diagnosticOnly"],
            "upstreamTests": receipt["upstreamTests"], "receiptSha256": pin, "steps": results}, indent=2) + "\n")
        print(step["name"] + ": exit " + str(step["exit"]), flush=True)
    for name, command in [("configure", configure), ("build",
        ["/usr/bin/cmake", "--build", str(build), "--",
         f"-j{args.jobs}", f"-l{args.load}"])]:
        step = run_logged(command, compiler_env, source, evidence, name, 1800)
        record(step)
        if step["exit"] or step["timedOut"]: return 1
    def native(row: str, name: str, function: str | None = None) -> dict:
        environment = bootstrap_environment(receipt, evidence / ("private-" + name),
            allow_failed_old=args.allow_failed_upstream_old_asan)
        if asan:
            environment["ASAN_OPTIONS"] = "detect_leaks=0:abort_on_error=1:halt_on_error=1:symbolize=1"
        xml = evidence / (name + ".xml")
        command = [str(build / ("qindaqt_audio_gain_experiment_" + row))]
        if function: command.append(function)
        command += ["-o", str(xml) + ",xml", "-o", "-,txt"]
        step = run_logged(command, environment, source, evidence, name, 240)
        if xml.is_file():
            try: step["qt"] = qt_totals(xml)
            except ET.ParseError: step["xmlIncomplete"] = True
            step["xmlSha256"] = hashlib.sha256(xml.read_bytes()).hexdigest()
        return step
    if old:
        step = native("borrowed_core_lifetime", "unpatched-red", "moduleFirstThenCoreStop")
        raw = Path(step["log"]).read_bytes()
        step["expectedAsanRed"] = (not step["timedOut"] and step["exit"] != 0
            and b"ERROR: AddressSanitizer: heap-use-after-free" in raw
            and b"module-loopback.c" in raw)
        record(step)
        return 0 if step["expectedAsanRed"] else 1
    for row in ROWS:
        function = "actualPinnedImageIsAdmitted" if row == "image_preflight" else None
        step = native(row, row, function); record(step)
        if not qt_passed(step):
            return 1
    # Poison only our copied configure-pinned receipt, never a package image or
    # host path. These direct methods must reject before private Core/children.
    saved = evidence / "cohort.saved"; shutil.move(pinned, saved)
    try:
        step = native("image_preflight", "receipt-withheld", "unavailableOrChangedReceiptRefuses")
        record(step)
        if not qt_passed(step): return 1
        pinned.write_bytes(receipt_bytes + b"\n")
        step = native("image_preflight", "receipt-changed", "unavailableOrChangedReceiptRefuses")
        record(step)
        if not qt_passed(step): return 1
    finally:
        pinned.unlink(missing_ok=True); shutil.move(saved, pinned)
    step = native("image_preflight", "receipt-restored", "actualPinnedImageIsAdmitted")
    record(step)
    return 0 if qt_passed(step) else 1


if __name__ == "__main__":
    sys.exit(main())

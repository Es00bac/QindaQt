#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicitly invoked Portage build-only cohort; never installs or starts audio."""
from __future__ import annotations
import argparse
import grp
import json
import os
from pathlib import Path
import subprocess
import sys
from functools import partial

from artifact_receipt import (OVERLAY, ORIGINAL_RECIPE, PATCHED_RECIPE,
                              artifact, make_receipt)
from process_evidence import run_logged
from portage_inputs import stage_inputs
from portage_environment import prepare_environment, admit_effective_environment
from portage_cohort import compile_and_observe, assemble_image
from evidence_io import snapshot

BASE = "b128176c5e2771be7719e9f72cd4a1f270bc4b7b"


def git(root: Path, *arguments: str) -> str:
    return subprocess.check_output(["/usr/bin/git", "-C", str(root), *arguments],
                                   text=True).strip()


def baseline(metadata: Path) -> dict[str, str]:
    result = {}
    for key in ("CFLAGS", "CXXFLAGS", "LDFLAGS", "USE", "FEATURES", "CHOST", "CBUILD"):
        data = (metadata / key).read_text()
        if len(data) > 65536 or "\0" in data:
            raise ValueError("invalid installed baseline")
        result[key] = data.strip()
    return result




def _warn_evidence(message: str) -> None:
    # A failed secondary diagnostic sink cannot replace the build exception.
    try:
        print(message, file=sys.stderr)
    except BaseException:
        pass


def build_attempt(recipe: Path, phases: list[str], environment: dict[str, str],
                  output: Path, inputs, record, *, name: str = "portage-build-only",
                  runner=None) -> dict:
    """Observe final inputs even when spawn/reap/logging or result recording fails.

    Inputs/record are borrowed synchronous collaborators. The original build
    exception remains authoritative; a second verification/write error must
    not erase it. Storage failure can prevent a receipt, but never yield success.
    """
    original = None
    try:
        run_process = run_logged if runner is None else runner
        step = run_process(["/usr/bin/ebuild", str(recipe), *phases],
                          environment, output, output, name, 7200)
        record(step)
        return step
    except BaseException as error:
        original = error
        raise
    finally:
        try:
            proof = inputs.verify()
            (output / "input-final-verification.json").write_text(
                json.dumps(proof, sort_keys=True, indent=2) + "\n")
        except BaseException:
            try:
                (output / "input-final-verification.json").write_text(
                    json.dumps({"verified": False, "reason": "cohort-input-changed-or-evidence-failed"})
                    + "\n")
            except BaseException:
                _warn_evidence("Final input evidence could not be written.")
            if original is None:
                raise
            _warn_evidence("Final input evidence failed; original build exception preserved.")


def main(*, runner=None) -> int:
    # Borrowed synchronous runner permits cooperative package cancellation.
    # Default composition/policy is unchanged; no callback may reap our child.
    run_process = run_logged if runner is None else runner
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("overlay", "gentoo", "distdir", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--mode", choices=["unpatched-asan-control", "patched-asan", "patched-normal"],
                        required=True)
    parser.add_argument("--installed-metadata", type=Path,
                        default=Path("/var/db/pkg/media-video/pipewire-1.6.8"))
    parser.add_argument("--asan-runtime", type=Path)
    parser.add_argument("--upstream-tests", action="store_true",
                        help="normal test variant adds only USE=test; release variant keeps baseline USE")
    parser.add_argument("--allow-failed-upstream-old-asan", action="store_true",
                        help="explicit OLD-only diagnostic image; upstream failures remain failed")
    args = parser.parse_args()
    if args.allow_failed_upstream_old_asan and args.mode != "unpatched-asan-control":
        parser.error("failed-suite exception is old-ASan-only")
    if os.geteuid() != 0:
        parser.error("Portage build must be explicitly invoked by the manager's package lane as root")
    if any(key.startswith("LD_") or key == "PIPEWIRE_MODULE_DIR" for key in os.environ):
        parser.error("remove ambient loader/module overrides before package invocation")
    overlay = args.overlay.resolve(strict=True)
    gentoo = args.gentoo.resolve(strict=True)
    if git(overlay, "rev-parse", "HEAD") != OVERLAY or git(overlay, "status", "--porcelain"):
        parser.error("use the clean exact reviewed b489 overlay worktree")
    if git(overlay, "merge-base", "--is-ancestor", BASE, OVERLAY) != "":
        parser.error("wrong authoritative master base")
    package = overlay / "media-video/pipewire"
    old = args.mode == "unpatched-asan-control"
    asan = args.mode != "patched-normal"
    if asan and args.asan_runtime is None:
        parser.error("ASan mode requires the explicitly selected compiler's sanitizer image")
    recipe = (gentoo / "media-video/pipewire/pipewire-1.6.8.ebuild" if old
              else package / "pipewire-1.6.8-r1.ebuild")
    if artifact(recipe)["sha256"] != (ORIGINAL_RECIPE if old else PATCHED_RECIPE):
        parser.error("exact Portage recipe changed")
    output = args.output.resolve()
    if output == Path("/") or output.exists():
        parser.error("use a fresh private qualification directory")
    group = grp.getgrnam("portage").gr_gid
    output.mkdir(parents=True, mode=0o750); os.chown(output, -1, group)
    inputs = stage_inputs(package / "Manifest", args.distdir.resolve(strict=True), output, group)
    (output / "input-admission.json").write_text(
        json.dumps(inputs.verify(), sort_keys=True, indent=2) + "\n")
    installed = baseline(args.installed_metadata.resolve(strict=True))
    (output / "installed-baseline.json").write_text(json.dumps(installed, indent=2) + "\n")
    import portage
    features = set(portage.settings.features)
    env_unset = portage.settings.get("ENV_UNSET", "")
    portage_uid = __import__("pwd").getpwnam("portage").pw_uid
    environment, test_variant = prepare_environment(output, group, portage_uid,
        installed, overlay, gentoo, inputs.distdir, features, env_unset,
        asan=asan, upstream_tests=args.upstream_tests)
    effective=admit_effective_environment(environment,portage.config)
    (output / "effective-private-bus-environment.json").write_text(
        json.dumps(effective,sort_keys=True,indent=2)+"\n")
    results = []
    def record(step: dict) -> None:
        results.append(step)
        (output / "results.json").write_text(json.dumps({
            "mode": args.mode, "upstreamTestVariant": test_variant,
            "actualBaselineUsePreserved": not test_variant,
            "makeopts": "-j8 -l8", "overlay": OVERLAY, "steps": results,
            "scope": "explicit Portage phases/image; no merge/world/services/installed acceptance"}, indent=2) + "\n")
        print(step["name"] + ": exit " + str(step["exit"]), flush=True)
    # Each attempt owns final verification. The failed original test phase
    # returns before any image action; strict observed evidence alone may
    # admit the explicit noauto diagnostic-only transition.
    pf = "pipewire-1.6.8" + ("" if old else "-r1")
    build = output / "tmp/portage/media-video" / pf
    qualification = compile_and_observe(recipe, environment, output, build, inputs,
        record, args.mode, test_variant, args.allow_failed_upstream_old_asan,
        partial(build_attempt, runner=run_process))
    step = assemble_image(recipe, environment, output, build, inputs, record, args.mode,
        qualification, args.allow_failed_upstream_old_asan,
        partial(build_attempt, runner=run_process))
    if step["exit"] or step["timedOut"]: return 1
    image = build / "image"; source = build / "work/pipewire-1.6.8"
    packages = sorted((output / "packages").rglob("*.gpkg.tar"))
    if len(packages) != 1:
        raise ValueError("expected exactly one privately created Portage package")
    verify = [sys.executable, str(Path(__file__).with_name("package_verify.py")),
              "--package", str(packages[0]), "--image", str(image),
              "--output", str(output / "package-verification")]
    if not asan: verify.append("--signed")
    step = run_process(verify, environment, output, output, "package-image-verification", 900)
    record(step)
    if step["exit"] or step["timedOut"]: return 1
    # Publish a usable receipt only after the actual package round trip passed.
    final = inputs.verify()
    (output / "input-publication-verification.json").write_text(
        json.dumps(final, sort_keys=True, indent=2) + "\n")
    final_record = snapshot(output / "input-publication-verification.json",
                            output / "suite-evidence/publication-inputs.json")
    receipt = make_receipt(image, source, recipe, inputs.snapshot / "pipewire-1.6.8.tar.bz2",
                           args.mode, "usr/lib64", args.asan_runtime,
                           output / "package-verification", qualification, final_record,
                           allow_failed_old=args.allow_failed_upstream_old_asan)
    receipt_path = output / "cohort.json"
    receipt_path.write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n")
    receipt_path.chmod(0o600)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

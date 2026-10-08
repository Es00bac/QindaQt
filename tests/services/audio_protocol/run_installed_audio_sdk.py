#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real production FILE_SET install; a separate client has no source rescue."""
import argparse
import json
import os
import shlex
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--native-makeopts", default=None)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    fixture = Path(__file__).resolve().parent
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="audio-sdk-", dir=args.output.resolve()))
    stage, build, consumer, poison = (root / n for n in
                                     ("stage", "production", "consumer", "poison"))
    env = os.environ.copy()
    for key in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH",
                "CMAKE_PREFIX_PATH", "QINDAQT_SOURCE_ROOT"):
        env.pop(key, None)
    env["DBUS_SESSION_BUS_ADDRESS"] = "unix:path=" + str(root / "absent-session-bus")
    env["DBUS_SYSTEM_BUS_ADDRESS"] = "unix:path=" + str(root / "absent-system-bus")
    results = []

    def run(label, command, expected=0):
        if "--build" in command and args.native_makeopts is not None:
            command += ["--"] + shlex.split(args.native_makeopts)
        with (root / (label + ".log")).open("wb") as log:
            result = subprocess.run(command, stdin=subprocess.DEVNULL,
                                    stdout=log, stderr=subprocess.STDOUT,
                                    env=env, timeout=120, check=False)
        results.append({"step": label, "argv": command, "exit": result.returncode})
        (root / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        if expected == "failure":
            if result.returncode == 0:
                raise RuntimeError(label + ": missing required header was rescued")
        elif result.returncode != expected:
            raise RuntimeError(label + ": unexpected exit " + str(result.returncode))

    run("configure-production", [args.cmake, "-S", str(fixture / "installed_sdk_fixture"),
        "-B", str(build), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_INSTALL_PREFIX=" + str(stage), "-DCMAKE_INSTALL_LIBDIR=lib",
        "-DCMAKE_INSTALL_INCLUDEDIR=include", "-DQINDAQT_SOURCE_ROOT=" + str(source)])
    # Plain CMake build preserves portable/native job policy.
    run("build-production", [args.cmake, "--build", str(build)])
    run("install-file-set", [args.cmake, "--install", str(build)])
    for module in (source / "src").rglob("include/qindaqt"):
        for header in module.rglob("*.h"):
            target = poison / header.relative_to(module.parent)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#error "QINDAQT_REQUIRED_STAGED_HEADER_MISSING"\n')
    run("configure-consumer", [args.cmake, "-S", str(fixture / "installed_sdk_consumer"),
        "-B", str(consumer), "-G", "Ninja", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DQINDAQT_STAGE=" + str(stage), "-DQINDAQT_POISON_INCLUDE=" + str(poison)])
    commands = (consumer / "compile_commands.json").read_text()
    if "/src/services/" in commands or str(source / "src") in commands:
        raise RuntimeError("consumer acquired a production source include path")
    run("build-consumer", [args.cmake, "--build", str(consumer)])
    run("run-consumer", [str(consumer / "audio_sdk_consumer")])
    required = stage / "include/qindaqt/services/audio_protocol/audio_console.h"
    saved = required.read_bytes()
    required.unlink()
    try:
        run("clean-consumer", [args.cmake, "--build", str(consumer), "--target", "clean"])
        run("required-header-poison", [args.cmake, "--build", str(consumer)], "failure")
        if "QINDAQT_REQUIRED_STAGED_HEADER_MISSING" not in (
                root / "required-header-poison.log").read_text():
            raise RuntimeError("negative failed without required-header poison")
    finally:
        required.write_bytes(saved)
    run("restored-build", [args.cmake, "--build", str(consumer)])
    run("restored-run", [str(consumer / "audio_sdk_consumer")])
    print("Audio SDK FILE_SET install / isolated consumer / poison / restore PASS")
    print("Evidence: " + str(root))


if __name__ == "__main__":
    main()

# Qt-only receipt harness: exact source freeze

- Time: 2026-10-08T12:56:14+00:00
- Existing worker base: `22d099fb259319de6b089314b67c9f4c358a5687`.
- Reviewed source/test: `e7a9e64e654abe8aabd67430760f55a1c3ba8ade`; Astra SOURCE ACCEPT `a413ac92ca89268b805dcc89a0da1e5e82fc4787`.
- Ignored harness: `.cache/audio-qt-only-receipt-harness-20261008/`. Its full CMake/dispatcher/command snapshots below are review evidence; no ordinary product/test/recipe/policy/helper/parser/README file changes.
- Exact source-freeze receipt SHA256 `0d6861708f129259223f0a6640df801f8db4843b0513bff0d647369c650676d2`. No CMake configure, compiler, runner, CTest, Core/PipeWire/Portage/image or host action has executed. Native counts are unavailable.
- Requested next gate: same Astra read-only admission of these exact ignored bytes/structured command, then a separate root short strict Qt-only fixture lease. No grant is inferred from R19 resource release.

The harness compiles exactly the reviewed test and three parser implementation files plus generated AUTOMOC. Four recursively consumed owning headers include the values-only gain_loaded_image.h; its implementation and every PipeWire/Core/runtime/image loader remain excluded. Only Qt6::Core/Test are linked. Repository C++20/Qt>=6.11 and the actual owning qindaqt_enable_warnings function apply with strictON, including-Werror. Readonly installed metadata shows GCC15.3.0 selected, Qt6.11.1 installed and about82GiB free; this is not a compiler invocation or ABI qualification. Actual tool/Core/Test/moc/configuration path/hash observations are included in the command pins. Two Python sources pass AST inspection; CMake/C++ syntax and execution are unqualified until the separate gate.

Future dispatch needs the exact command SHA and explicit execute-authorized-fixture flag. It refuses missing review hash, changed pin, reused/outside run directory and insufficient2GiB scratch. It starts no child until input checks pass. It uses an explicit environment, private0700 HOME/XDG/tmp directories and nonexistent session/system bus addresses. No host values, LD_PRELOAD/ASAN/PipeWire/QML/Qt plugin/display/Wayland or compiler-cache variables are inherited. Bounded regular-file descriptor/version observations are not writer exclusion or filesystem/parser deadline claims.

Per-file output limit64MiB applies to compiler files and retained raw logs; hitting it fails the gate. Core dumps are disabled. Configure120s/build300s/CTest70s are sequential; test itself60s. Existing unchanged process_evidence.run_logged reserves each owned leader with WNOWAIT through TERM/KILL group cleanup and reaping. First nonzero, timeout or exception stops all later phases with no retry. A final input observation/result receipt runs on every attempted phase outcome and preserves the original failure even if final verification fails. Evidence/scratch is retained, never merged/installed, and any cleanup breach remains failure without signaling a reused PID.

Predicted full fixture completion is34 authored behavior rows plus init/cleanup=36 passes, zero fail/skip/blacklist and exactly11 named Qt functions. These are assertions in the prospective command, not observed native totals. Any absent/incomplete Qt XML refuses. Compile commands, build/CTest/raw Qt text/XML and exact process/entry/final source receipts are retained by future dispatch. Actual complete-suite fixture pass is still mandatory before any separately authorized original-ASan lifetime control.


Exact CMakeLists.txt SHA256 cd2064b9084424729a1dedd6ed0016376db27b9f4a31c197d01868ab4c36b02f:

```cmake
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)
project(QindaQtReceiptQualificationOnly LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_PATH_PREFIX OFF)
set(QINDAQT_ENABLE_STRICT_WARNINGS ON)
if(NOT IS_ABSOLUTE "${QINDAQT_RECEIPT_SOURCE_ROOT}")
    message(FATAL_ERROR "An exact absolute reviewed source root is required")
endif()
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
include("${QINDAQT_RECEIPT_SOURCE_ROOT}/cmake/QindaQtCompiler.cmake")
set(receipt_owner "${QINDAQT_RECEIPT_SOURCE_ROOT}/tests/services/audio_service")
# AGENT-GUARD: This harness compiles exactly four hand-written parser/fixture
# translation units. gain_loaded_image.h is values only; never link its .cpp,
# a qualified-image/runtime helper, PipeWire or the production Audio target.
qt_add_executable(qindaqt_audio_receipt_qualification_qt_only
    "${receipt_owner}/tst_console_qualification_receipt.cpp"
    "${receipt_owner}/support/gain_qualification_evidence.cpp"
    "${receipt_owner}/support/gain_qualification_receipt.cpp"
    "${receipt_owner}/support/gain_upstream_suite.cpp"
    "${receipt_owner}/support/gain_qualification_evidence.h"
    "${receipt_owner}/support/gain_qualification_receipt.h"
    "${receipt_owner}/support/gain_upstream_suite.h"
    "${receipt_owner}/support/gain_loaded_image.h")
target_include_directories(qindaqt_audio_receipt_qualification_qt_only PRIVATE "${receipt_owner}")
target_compile_definitions(qindaqt_audio_receipt_qualification_qt_only PRIVATE QT_NO_KEYWORDS)
target_link_libraries(qindaqt_audio_receipt_qualification_qt_only PRIVATE Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_audio_receipt_qualification_qt_only)
enable_testing()
add_test(NAME qindaqt.audio-receipt-qualification-qt-only
    COMMAND qindaqt_audio_receipt_qualification_qt_only
        -o "${CMAKE_BINARY_DIR}/receipt-tests.txt,txt"
        -o "${CMAKE_BINARY_DIR}/receipt-tests.xml,xml")
set_tests_properties(qindaqt.audio-receipt-qualification-qt-only PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60)
```

Exact run_fixture.py SHA256 05b586090f7dcdcf20be63f4c1a65328adc08366ddbd3edf8464d681cd29bfd4:

```python
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reviewed ignored dispatcher; execution needs a separate explicit root lease."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import stat
import types
import xml.etree.ElementTree as ET

def read_regular(path: Path) -> bytes:
    resolved = path.resolve(strict=True)
    fd = os.open(resolved, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
    with os.fdopen(fd, "rb") as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or not 0 < before.st_size <= 256 * 1024**2:
            raise RuntimeError("bounded regular input required")
        chunks = []
        remaining = before.st_size
        while remaining:
            block = stream.read(min(65536, remaining))
            if not block: raise RuntimeError("input contracted while read")
            chunks.append(block)
            remaining -= len(block)
        if stream.read(1): raise RuntimeError("input grew while read")
        after = os.fstat(stream.fileno())
        named = os.stat(resolved, follow_symlinks=False)
    version = lambda item: (item.st_dev, item.st_ino, item.st_size, item.st_mode,
                            item.st_nlink, item.st_mtime_ns, item.st_ctime_ns)
    if version(before) != version(after) or version(after) != version(named):
        raise RuntimeError("observed input version changed")
    return b"".join(chunks)

def digest(path: Path) -> str:
    return hashlib.sha256(read_regular(path)).hexdigest()

def pins_match(command: dict) -> bool:
    return all(Path(row["path"]).resolve(strict=True) == Path(row["resolved"])
               and digest(Path(row["path"])) == row["sha256"]
               for row in command["pins"])

def qt_totals(path: Path) -> dict:
    document = ET.fromstring(read_regular(path))
    counts = {"pass": 0, "fail": 0, "skip": 0, "blacklist": 0}
    for incident in document.iter("Incident"):
        kind = incident.attrib.get("type", "")
        if kind in counts: counts[kind] += 1
        else: raise ValueError("unknown Qt incident")
    counts["functions"] = [item.attrib["name"] for item in document.findall("TestFunction")]
    return counts

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--command", type=Path, required=True)
    parser.add_argument("--expected-command-sha256", required=True)
    parser.add_argument("--run-root", type=Path, required=True)
    parser.add_argument("--execute-authorized-fixture", action="store_true")
    args = parser.parse_args()
    if not args.execute_authorized_fixture:
        raise RuntimeError("separate root execution lease is required")
    command_path = args.command.resolve(strict=True)
    command_bytes = read_regular(command_path)
    if hashlib.sha256(command_bytes).hexdigest() != args.expected_command_sha256:
        raise RuntimeError("reviewed command digest changed")
    command = json.loads(command_bytes)
    harness = command_path.parent
    run = args.run_root.resolve()
    if run.parent != harness / "runs" or run.exists():
        raise RuntimeError("a fresh directly owned ignored run directory is required")
    run.parent.mkdir(mode=0o700, exist_ok=True)
    run.mkdir(mode=0o700)
    evidence = run / "evidence"
    evidence.mkdir(mode=0o700)
    result = {"commandSHA256": args.expected_command_sha256, "steps": [],
              "started": False, "inputBeforeVerified": False, "inputFinalVerified": False,
              "success": False, "failure": ""}
    outcome = 1
    try:
        if command["schema"] != 1 or not pins_match(command):
            raise RuntimeError("reviewed source/harness/tool input changed")
        result["inputBeforeVerified"] = True
        available = os.statvfs(run).f_bavail * os.statvfs(run).f_frsize
        result["availableBytesAtStart"] = available
        if available < command["resourceLimits"]["minimumFreeBytes"]:
            raise RuntimeError("private scratch headroom is insufficient")
        # Per-file bound applies to retained logs AND compiler output; hitting
        # it fails this gate. No sanitizer, package or production policy changes.
        limit = command["resourceLimits"]["maximumFileBytes"]
        resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        context = {"source": command["sourceRoot"], "harness": str(harness), "run": str(run)}
        environment = {key: value.format_map(context)
                       for key, value in command["environment"].items()}
        for name in command["privateDirectories"]:
            (run / name).mkdir(mode=0o700)
        (evidence / "effective-child-environment.json").write_text(
            json.dumps(environment, indent=2) + "\n")
        helper = Path(command["processHelper"])
        helper_bytes = read_regular(helper)
        helper_pin = next(row for row in command["pins"] if row["path"] == str(helper))
        if hashlib.sha256(helper_bytes).hexdigest() != helper_pin["sha256"]:
            raise RuntimeError("owned runner changed before loading")
        module = types.ModuleType("receipt_owned_process")
        exec(compile(helper_bytes, str(helper), "exec"), module.__dict__)
        for item in command["steps"]:
            argv = [argument.format_map(context) for argument in item["argv"]]
            result["started"] = True
            step = module.run_logged(argv, environment, run, evidence,
                                     item["name"], item["timeoutSeconds"])
            result["steps"].append(step)
            (evidence / "steps.json").write_text(json.dumps(result["steps"], indent=2) + "\n")
            if step["exit"] != 0 or step["timedOut"] or not step["leaderReservedThroughCleanup"]:
                raise RuntimeError("first owned command failure; no retry or later step")
        totals = qt_totals(run / "build" / "receipt-tests.xml")
        result["qt"] = totals
        expected = command["expectedQt"]
        if ({key: totals[key] for key in ("pass", "fail", "skip", "blacklist")}
                != expected["incidents"]
                or sorted(totals["functions"]) != sorted(expected["functions"])):
            raise RuntimeError("full synthetic Qt fixture result is incomplete")
        result["success"] = True
        outcome = 0
    except Exception as failure:
        result["failure"] = type(failure).__name__ + ": " + str(failure)
    finally:
        try:
            result["inputFinalVerified"] = (
                digest(command_path) == args.expected_command_sha256 and pins_match(command))
            if not result["inputFinalVerified"]:
                result["success"] = False
                outcome = 1
        except Exception as final_failure:
            result["finalVerificationError"] = type(final_failure).__name__ + ": " + str(final_failure)
            result["success"] = False
            outcome = 1
        (evidence / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    return outcome

if __name__ == "__main__":
    raise SystemExit(main())
```

Exact command.json SHA256 431aa563643077e08b0453126c6927f344f25dec56b022f57dd0e5709d60349e:

```json
{
  "schema": 1,
  "sourceCandidate": "e7a9e64e654abe8aabd67430760f55a1c3ba8ade",
  "scope": "Only synthetic Qt/Core receipt parsing; no image/runtime/Core/PipeWire/package/host action",
  "sourceRoot": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008",
  "sourceFiles": [
    "tests/services/audio_service/tst_console_qualification_receipt.cpp",
    "tests/services/audio_service/support/gain_qualification_evidence.cpp",
    "tests/services/audio_service/support/gain_qualification_receipt.cpp",
    "tests/services/audio_service/support/gain_upstream_suite.cpp"
  ],
  "owningHeaders": [
    "tests/services/audio_service/support/gain_loaded_image.h",
    "tests/services/audio_service/support/gain_qualification_evidence.h",
    "tests/services/audio_service/support/gain_qualification_receipt.h",
    "tests/services/audio_service/support/gain_upstream_suite.h"
  ],
  "processHelper": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/console_gain_experiment/process_evidence.py",
  "pins": [
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/tst_console_qualification_receipt.cpp",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/tst_console_qualification_receipt.cpp",
      "sha256": "0b10d540305fff6793496aceb7b400597b118bf88ea2d1cdba8242d181cecb42"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_evidence.cpp",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_evidence.cpp",
      "sha256": "052abb511cb02c7ed48a236a128a48cf088f975da0660ffa6f45de21c4219e9a"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_receipt.cpp",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_receipt.cpp",
      "sha256": "7081aee3d4df95fb063ccdadff963bcec424928932786c5abb0061adcaf4f3f2"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_upstream_suite.cpp",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_upstream_suite.cpp",
      "sha256": "193109860437d5a47fe70dacea639554b17bfcad99b65d55c525f5e4041ebf31"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_loaded_image.h",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_loaded_image.h",
      "sha256": "886fc7718a02ef50efe85427822e3d54fb74d3c5b865581f42050c0082060ea8"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_evidence.h",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_evidence.h",
      "sha256": "0a34156e418f019fb9fba7b991fb54ce14c1cfb6b613c87eb1420d778b96eb14"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_receipt.h",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_qualification_receipt.h",
      "sha256": "d75e3621ff0a56b64d4a0d6acacf5f039a4c58acd8d6679a28bd88586053b145"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_upstream_suite.h",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/support/gain_upstream_suite.h",
      "sha256": "6e6f57677621d58b6726670ac83e5397df5baf6ce5a4efdea1cc556aed42cc75"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/cmake/QindaQtCompiler.cmake",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/cmake/QindaQtCompiler.cmake",
      "sha256": "3a76c0765046d019d653bfc4b1b48b9d9068cbfde4caaf9dcffe79cb84f5d298"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/console_gain_experiment/process_evidence.py",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/tests/services/audio_service/console_gain_experiment/process_evidence.py",
      "sha256": "842372d018a9e3e7a89a7ccb1f9369beb1c49f9960d8be71c52a28deea5ab42c"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/.cache/audio-qt-only-receipt-harness-20261008/CMakeLists.txt",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/.cache/audio-qt-only-receipt-harness-20261008/CMakeLists.txt",
      "sha256": "cd2064b9084424729a1dedd6ed0016376db27b9f4a31c197d01868ab4c36b02f"
    },
    {
      "path": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/.cache/audio-qt-only-receipt-harness-20261008/run_fixture.py",
      "resolved": "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/.cache/audio-qt-only-receipt-harness-20261008/run_fixture.py",
      "sha256": "05b586090f7dcdcf20be63f4c1a65328adc08366ddbd3edf8464d681cd29bfd4"
    },
    {
      "path": "/usr/bin/cmake",
      "resolved": "/usr/bin/cmake",
      "sha256": "37143b4dd6609c1c95ce42b3f9db125e3c601e91f62696c48641d766bfe0e1d3"
    },
    {
      "path": "/usr/bin/ctest",
      "resolved": "/usr/bin/ctest",
      "sha256": "86ff100ac5f12c2c2bc8e13566d1c01054da7079632b13c025b342bafcde4774"
    },
    {
      "path": "/usr/bin/ninja",
      "resolved": "/usr/bin/ninja-reference",
      "sha256": "6deb33565927e63142576264c66e16ad50574b1d05424d196fc9cfef479b488f"
    },
    {
      "path": "/usr/bin/g++",
      "resolved": "/usr/x86_64-pc-linux-gnu/gcc-bin/15/x86_64-pc-linux-gnu-g++",
      "sha256": "41a337096e59704d5c86f9a3a36eb228d3cc80de039aeefc927cc8f21b50966e"
    },
    {
      "path": "/usr/lib/python-exec/python3.14/python3",
      "resolved": "/usr/bin/python3.14",
      "sha256": "07080f01ff9495ab7341fdf46ea4cba406d8ee5fdd38e86b35ea9a9cab85ec60"
    },
    {
      "path": "/usr/lib64/qt6/libexec/moc",
      "resolved": "/usr/lib64/qt6/libexec/moc",
      "sha256": "31840737dd20970f9cfe85bb3dbf4210e921c0a51eb1971c5f784a86ceb17366"
    },
    {
      "path": "/usr/lib64/libQt6Core.so.6",
      "resolved": "/usr/lib64/libQt6Core.so.6.11.1",
      "sha256": "950ddcebe59542937122afc64aa63da94efa90cd5a96f0f6a16903d08f98d3d1"
    },
    {
      "path": "/usr/lib64/libQt6Test.so.6",
      "resolved": "/usr/lib64/libQt6Test.so.6.11.1",
      "sha256": "793315fc226018f985b4d1795645fed36e0c8a97f679a903a2dd01f72f68bfb4"
    },
    {
      "path": "/usr/lib64/cmake/Qt6/Qt6Config.cmake",
      "resolved": "/usr/lib64/cmake/Qt6/Qt6Config.cmake",
      "sha256": "8a90ef00a83ab650b6d9065ed87b6aa29e6acc192ffb6db90fcd148fa36042d8"
    },
    {
      "path": "/usr/lib64/cmake/Qt6Core/Qt6CoreConfig.cmake",
      "resolved": "/usr/lib64/cmake/Qt6Core/Qt6CoreConfig.cmake",
      "sha256": "34647347a7f25250536dc9bb06c63aa00cbbe76633e575d6aefcafa571b4a82b"
    },
    {
      "path": "/usr/lib64/cmake/Qt6Test/Qt6TestConfig.cmake",
      "resolved": "/usr/lib64/cmake/Qt6Test/Qt6TestConfig.cmake",
      "sha256": "1669f6b11b9bb2510119496473887420508744a2961a770143546e896decd326"
    },
    {
      "path": "/usr/lib64/cmake/Qt6Test/Qt6TestDependencies.cmake",
      "resolved": "/usr/lib64/cmake/Qt6Test/Qt6TestDependencies.cmake",
      "sha256": "999b7b55c3a80aaa24be65fcc8bfea2347ae5d7b1990d9ec12bc270f37646e12"
    },
    {
      "path": "/usr/lib64/cmake/Qt6Test/Qt6TestTargets.cmake",
      "resolved": "/usr/lib64/cmake/Qt6Test/Qt6TestTargets.cmake",
      "sha256": "9771cfb181d51629808d723fb9d04ea31898ed3a6e931f7a0614b52fbee77a1e"
    },
    {
      "path": "/usr/lib64/cmake/Qt6Core/Qt6CoreConfigVersion.cmake",
      "resolved": "/usr/lib64/cmake/Qt6Core/Qt6CoreConfigVersion.cmake",
      "sha256": "ac8e2dba5451c44386bac3b2291552793f0ed6b6818825983b2425dbf7c703b0"
    },
    {
      "path": "/usr/lib64/cmake/Qt6Test/Qt6TestConfigVersion.cmake",
      "resolved": "/usr/lib64/cmake/Qt6Test/Qt6TestConfigVersion.cmake",
      "sha256": "d48eb8c635e6516cf5bf0dc3f468ed171ecd249631d35eed250007ee91c688a9"
    }
  ],
  "privateDirectories": [
    "home",
    "runtime",
    "config",
    "cache",
    "data",
    "tmp"
  ],
  "environment": {
    "PATH": "/usr/bin:/bin",
    "LANG": "C.UTF-8",
    "LC_ALL": "C.UTF-8",
    "HOME": "{run}/home",
    "XDG_RUNTIME_DIR": "{run}/runtime",
    "XDG_CONFIG_HOME": "{run}/config",
    "XDG_CACHE_HOME": "{run}/cache",
    "XDG_DATA_HOME": "{run}/data",
    "TMPDIR": "{run}/tmp",
    "DBUS_SESSION_BUS_ADDRESS": "unix:path={run}/absent-session",
    "DBUS_SYSTEM_BUS_ADDRESS": "unix:path={run}/absent-system",
    "MAKEOPTS": "-j24 -l24"
  },
  "resourceLimits": {
    "minimumFreeBytes": 2147483648,
    "maximumFileBytes": 67108864,
    "coreDumpBytes": 0
  },
  "steps": [
    {
      "name": "configure",
      "timeoutSeconds": 120,
      "argv": [
        "/usr/bin/cmake",
        "-S",
        "{harness}",
        "-B",
        "{run}/build",
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DCMAKE_CXX_COMPILER=/usr/x86_64-pc-linux-gnu/gcc-bin/15/x86_64-pc-linux-gnu-g++",
        "-DCMAKE_MAKE_PROGRAM=/usr/bin/ninja-reference",
        "-DCMAKE_CXX_COMPILER_LAUNCHER=",
        "-DQINDAQT_RECEIPT_SOURCE_ROOT={source}"
      ]
    },
    {
      "name": "build",
      "timeoutSeconds": 300,
      "argv": [
        "/usr/bin/cmake",
        "--build",
        "{run}/build",
        "--target",
        "qindaqt_audio_receipt_qualification_qt_only",
        "--parallel",
        "24",
        "--",
        "-l24"
      ]
    },
    {
      "name": "ctest",
      "timeoutSeconds": 70,
      "argv": [
        "/usr/bin/ctest",
        "--test-dir",
        "{run}/build",
        "-R",
        "^qindaqt.audio-receipt-qualification-qt-only$",
        "--output-on-failure",
        "--output-junit",
        "{run}/evidence/ctest.xml",
        "--timeout",
        "60"
      ]
    }
  ],
  "expectedQt": {
    "incidents": {
      "pass": 36,
      "fail": 0,
      "skip": 0,
      "blacklist": 0
    },
    "functions": [
      "initTestCase",
      "cleanupTestCase",
      "completeSuitesReachRawParsers",
      "completeFailedOldNeedsExplicitOptIn",
      "completeFailedFixedRefuses",
      "completeSuitePoisonsRefuse",
      "normalReleaseMakesNoTestQualificationClaim",
      "unknownMissingAndMistypedFlagsRefuse",
      "failedOptInCannotAdmitFixedOrContradictoryEvidence",
      "wrongPhaseAndReplacedFinalInputsRefuse",
      "fifoReadRefusesBeforeAnyCoreOrWriter"
    ]
  },
  "executionState": "NOT EXECUTED; independent source admission and explicit root compiler/fixture lease required"
}
```

Exact dispatch.json SHA256 5b551208d04a000a10dd746479a35c61f562341c82456b45a34897eb954ef917:

```json
{
  "argv": [
    "/usr/bin/python3.14",
    "-I",
    "-S",
    "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/.cache/audio-qt-only-receipt-harness-20261008/run_fixture.py",
    "--command",
    "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/.cache/audio-qt-only-receipt-harness-20261008/command.json",
    "--expected-command-sha256",
    "431aa563643077e08b0453126c6927f344f25dec56b022f57dd0e5709d60349e",
    "--run-root",
    "/home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-gain-20261008/.cache/audio-qt-only-receipt-harness-20261008/runs/LEASE_TIMESTAMP",
    "--execute-authorized-fixture"
  ],
  "execution": "prospective only, no grant; replace only LEASE_TIMESTAMP with fresh directly-owned directory name",
  "commandSHA256": "431aa563643077e08b0453126c6927f344f25dec56b022f57dd0e5709d60349e"
}
```

# Exact-candidate repair recheck — Audio applet production composition

- Reviewer persona: Barbara Simons (`barbara-simons`), independent shell-composition reviewer
- Provider/model: OpenAI Codex, GPT-5 (actual executing agent identity)
- Candidate SHA: `caaf7d9a15dd7a936eaee2e8ff83332a5c945970`
- Candidate tree SHA: `0eb16617c18eb0a668614fdeb5543297adf5b4ae`
- Parent SHA: `6484ad8f9e43ca1b7fc40658a9ae26e1c109c9b8`
- Base/product ancestor SHA: `807b1aeed22de3852b18c1609bd9eae04b2b67d9`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-production-glm-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-audioapplet-r2-codex`

The worktree was detached at the exact candidate and clean before review. It
remained clean after all builds, tests, poison reproductions, and static gates.
All scratch mutations stayed under the assigned build root.

## Findings ledger

### P0 — none

### P1 — none

### P2 — none

### P3 — none

## Prior finding closure

### P1-1 — closed

The immutable candidate contains both required QML test-stub blobs:

    tests/shell/qml/imports/QindaQt/Shell/AudioApplet/AudioApplet.qml
    tests/shell/qml/imports/QindaQt/Shell/AudioApplet/qmldir

`git status --porcelain --ignored -- src tests data docs` returned no output.
The prior failing row passed from this clean tree in the shell/runtime selector
in both profiles:

    Debug:  qindaqt.notification-center-applet-offscreen — passed
    Release: qindaqt.notification-center-applet-offscreen — passed

The complete shell/runtime selector passed 6/6 in Debug and 6/6 in Release.
The focused Audio selector, including `qindaqt.audio-applet-offscreen`, passed
6/6 in both profiles. Direct offscreen runs with `QT_FATAL_WARNINGS=1` passed
3/3 in both profiles.

### P2-1 — closed

The pure boundary script at
`tests/shell/audio_applet/check_boundary.cmake:112-169` now uses
`POISON_ROOT`. The normal invocation truthfully reported 3 source files and 0
poison rejections; the poisoned invocation truthfully reported 3 source files
and 4 poison rejections, then removed its fixture root.

The four cases use separate copied trees. Independent reviewer fixtures proved
that every individual mutation is rejected for its intended reason:

- transport: `#include <QtDBus/QDBusConnection>` — exit 1; `QtDBus`, `QDBus`,
  and non-boundary include violations
- service-internal:
  `#include <qindaqt/services/audio_service/resident_audio_service.h>` — exit
  1; non-boundary include violation
- QML: `#include <QtQml/QQmlEngine>` — exit 1; `QtQml` and non-boundary include
  violations
- QObject: `class QObject;` — exit 1; production `QObject` violation

Thus the reported count is neither shared-fixture leakage nor a count of
successful nested gates.

### Prior P3s — addressed

- `src/shell/runtime/shellruntimeapplication.h:144-149` now identifies
  unconditional `resetRuntime()` ordering as the lifetime protection and
  explicitly warns that declaration order would be unsafe.
- `tests/shell/audio_applet/tst_audio_applet_controller.cpp:616-638` now sends
  an old-owner reply after replacement, verifies the controller remains
  loading with empty rows, and only then accepts new-owner truth. The focused
  test function passed 3/3 in both profiles.

## Scope audit

`git diff --name-status 6484ad8..caaf7d9` contains exactly the seven declared
repair paths:

    M docs/wiki/development/testing-harness.md
    M docs/wiki/shell/audio-applet.md
    M src/shell/runtime/shellruntimeapplication.h
    M tests/shell/audio_applet/check_boundary.cmake
    M tests/shell/audio_applet/tst_audio_applet_controller.cpp
    A tests/shell/qml/imports/QindaQt/Shell/AudioApplet/AudioApplet.qml
    A tests/shell/qml/imports/QindaQt/Shell/AudioApplet/qmldir

The two additional paths visible in `807b1ae..caaf7d9` are the handoff message
and worker record introduced by parent `6484ad8`; `git diff --name-status
807b1ae..6484ad8` confirms they are workflow-only. No Power or Bluetooth
product path changed. The repaired docs describe the executed four-poison and
old-owner-reply evidence without expanding product claims.

## Commands and results

### Identity and cleanliness

    git rev-parse HEAD
    → caaf7d9a15dd7a936eaee2e8ff83332a5c945970 (match)

    git rev-parse HEAD^{tree}
    → 0eb16617c18eb0a668614fdeb5543297adf5b4ae

    git rev-parse HEAD^
    → 6484ad8f9e43ca1b7fc40658a9ae26e1c109c9b8

    git merge-base 807b1ae caaf7d9
    → 807b1aeed22de3852b18c1609bd9eae04b2b67d9

    git status --porcelain
    → empty before and after review

    git status --porcelain --ignored -- src tests data docs
    → empty before and after review

### Configure

Both exact recipe invocations exited 0:

    cmake -S . -B <ROOT>/debug -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
      -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
      -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
      -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

    cmake -S . -B <ROOT>/release -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
      -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
      -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

### Build

The following command was run for both `<profile>=debug` and `release`; each
exited 0 with 593/593 steps:

    cmake --build <ROOT>/<profile> --parallel 3 --target \
      qindaqt_shell_audio_applet qindaqt_shell_audio_applet_runtime \
      qindaqt_audio_applet_model_tests \
      qindaqt_audio_applet_controller_tests \
      qindaqt_audio_applet_qml_tests qindaqt-shell qindaqt-shell-preview \
      qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
      qindaqt_applet_instance_resolver_tests \
      qindaqt_applet_host_policy_tests qindaqt_applet_host_handshake_tests \
      qindaqt_applet_host_lifecycle_tests qindaqt_shell_runtime_options_tests \
      qindaqt_power_applet_qml_tests qindaqt_power_applet_controller_tests \
      qindaqt_bluetooth_applet_qml_tests \
      qindaqt_bluetooth_applet_controller_tests

### CTest selectors

Each command used `--output-on-failure --no-tests=error` and exited 0 in both
Debug and Release:

    ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.audio-applet-' \
      --output-on-failure --no-tests=error
    → Debug 6/6; Release 6/6

    ctest --test-dir <ROOT>/<profile> \
      -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution)$' \
      --output-on-failure --no-tests=error
    → Debug 3/3; Release 3/3

    ctest --test-dir <ROOT>/<profile> \
      -R '^qindaqt\.(shell-runtime-catalog|shell-runtime-options|notification-center-applet-offscreen|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$' \
      --output-on-failure --no-tests=error
    → Debug 6/6; Release 6/6

    ctest --test-dir <ROOT>/<profile> \
      -R '^qindaqt\.(power|bluetooth)-applet-(offscreen|controller)$' \
      --output-on-failure --no-tests=error
    → Debug 4/4; Release 4/4

Direct focused checks:

    QT_FATAL_WARNINGS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
      <ROOT>/<profile>/tests/shell/audio_applet/qindaqt_audio_applet_qml_tests
    → Debug 3/3; Release 3/3; both exit 0

    <ROOT>/<profile>/tests/shell/audio_applet/qindaqt_audio_applet_controller_tests \
      ownerReplacementClearsTruthAndPendingWithoutReplay
    → Debug 3/3; Release 3/3; both exit 0

### Boundary controls

    cmake -DSOURCE_ROOT=$PWD \
      -P tests/shell/audio_applet/check_boundary.cmake
    → exit 0: "passed (3 files and 0 poison rejections)"

    cmake -DSOURCE_ROOT=$PWD \
      -DPOISON_ROOT=<ROOT>/boundary-poison-check \
      -P tests/shell/audio_applet/check_boundary.cmake
    → exit 0: "passed (3 files and 4 poison rejections)"
    → `<ROOT>/boundary-poison-check` absent afterward

For each independent reviewer fixture, the three gate inputs were copied to
`<ROOT>/manual-poisons/<case>/`, only the named mutation above was appended,
and this command was run:

    cmake -DSOURCE_ROOT=<ROOT>/manual-poisons/<case> \
      -DAUDIO_APPLET_PURE_POLICY_SKIP_POISON=ON \
      -P tests/shell/audio_applet/check_boundary.cmake
    → transport exit 1; service-internal exit 1; qml exit 1; qobject exit 1

### Static gates

    ./tools/validate-docs
    → exit 0: Validated 117 Markdown documents and mkdocs.yml navigation.

    /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build \
      --strict --site-dir <ROOT>/site
    → exit 0; documentation built with no warnings

    ./tools/check-source-shape
    → exit 0; checked 1,786 files, skipped 0
    → warning only: `tst_audio_applet_controller.cpp` is 563 non-blank lines,
      the already-disclosed test-file decomposition-review threshold

    git diff --check
    git diff --check 807b1ae..caaf7d9
    → both exit 0, no output

No JSON changed in the repair commit. As an adjacent integrity recheck, both
original Audio composition JSON files still parsed successfully:

    python3 -m json.tool data/applets/audio.json
    python3 -m json.tool data/profiles/qindaqt.json
    → both exit 0

### Intentionally not run

Per the review brief, no `tests/session` nested-compositor row, host D-Bus
system/session service, hardware, uinput, or network test was run. The selected
installed-package row uses only its staged/offscreen package proof.

## Verdict

Both blocking findings and both precision findings from the prior review are
closed on the exact immutable candidate. The repair is scope-bounded, its tests
are non-vacuous, and the requested Debug, Release, adjacent, and static evidence
all passes.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0

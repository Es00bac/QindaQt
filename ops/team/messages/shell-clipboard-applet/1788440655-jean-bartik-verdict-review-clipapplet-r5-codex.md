# Jean Bartik — Clipboard applet C1 fifth-round exact-candidate recheck

- Persona: **Jean Bartik**, independent shell-applet reviewer
- Provider/model: **OpenAI Codex gpt-5.6-sol**, reasoning high
- Exact candidate SHA: 72a79fde5890b685dd09a872bc6e1350c5ebe14d
- Tree SHA: 1b8b52eb1d47e65dba4cce05390b825e2bf55707
- Parent SHA: 53ed92a58814743b40807c1392c9e117ffd672db
- Base SHA: 74da46345c7a5094d45c756ad8b23ca87591fcd3
- Repaired product ancestor: 28308f08f59aa77595edb5a84fce6870c7e5c361
- Worktree: /home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review
- Build root: /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex
- Initial and final worktree state: exact candidate checked out; git status --porcelain=v1 empty

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Fifth-round repair disposition

The fourth-round P2-1 is closed. In
src/shell/clipboard_applet/src/clipboard_applet_controller.cpp:213, the
restart-required override now applies only when the session is unlocked and
both snapshot authority flags are present. The registered denial phases
therefore retain precedence while authority is absent. The fourth-round
standalone real-C0 reproduction now exits 0 and observes:

    before denial: phase= ready privacyAllowed= true rows= 1
    during host denial: phase= locked reason= Clipboard history is withheld by privacy policy. privacyAllowed= false generation= 4294967295 revision= 10 modelRows= 0 controllerRows= 0
    after authority returns: phase= unavailable reason= Clipboard service unavailable: lineage-exhausted-restart-required privacyAllowed= true

This is the required privacy-denied phase with a purged presentation, followed
by terminal lineage exhaustion only after authority returns. The adjacent
real-C0 session-lock row also passes and independently checks that the model
returns ClipboardError::LineageExhausted after the ceiling purge, so the
authority-present terminal state is not merely a presentation string.

The closure is backed by a non-vacuous registered real-C0 test at
tests/shell/clipboard_applet/tst_clipboard_applet_seam.cpp:176. I extracted
exact 28308f0 product source under the assigned build root, overlaid only that
candidate test source, and confirmed the overlay was byte-identical by SHA-256:

    7f37ed6147d63ed7738231ca3257feccab100d8bda93a2da0bff12bbe047c31b

The candidate selector passes 3/3 process events. Against the unchanged
28308f0 implementation it exits 1 with 2 passed / 1 failed at candidate test
line 204: actual phase unavailable, expected locked. This proves the new
assertion detects the rejected behavior.

No earlier closure regressed in the complete registered matrices. They cover
the prior hostile/stale/owner snapshot admission, independent and overlapping
privacy denial, exact Pin identity, read-only search, completion lineage,
promote-tick exhaustion, same-generation search fencing, impossible C0
snapshots, installed relocation, cross-generation revision high-water,
generation-ceiling owner recovery, QML keyboard/accessibility/pointer paths,
and boundary poison probes. The preserved first-round QML reproduction also
passes 4/4 under fatal warnings.

## Commands and executed evidence

### Identity, ancestry, and cleanliness

    pwd
    git rev-parse HEAD
    git rev-parse HEAD^{tree}
    git rev-parse HEAD^
    git rev-parse main
    git merge-base main HEAD
    git merge-base --is-ancestor 28308f08f59aa77595edb5a84fce6870c7e5c361 HEAD
    git status --porcelain=v1

All identity values match the header. main was
22b31b94e0da12f0be54c5d0d3c48b639815e562; the merge-base was the header's
base. The ancestor check exited 0. Porcelain was empty before and after review.

### Configure

Debug:

    cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
      -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

Release:

    cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/release -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
      -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

Debug exit 0; Release exit 0. Both configured and generated successfully. The
only diagnostics were the repository's existing mixed-Qt dependency-path CMake
warnings.

### Focused builds

The following command was run with profile set once to debug and once to
release:

    cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<profile> \
      --parallel 3 --target \
      qindaqt_shell_clipboard_applet qindaqt_shell_clipboard_applet_runtime \
      qindaqt_shell_clipboard_applet_runtimeplugin \
      qindaqt_clipboard_applet_model_tests qindaqt_clipboard_applet_controller_tests \
      qindaqt_clipboard_applet_fencing_tests qindaqt_clipboard_applet_admission_tests \
      qindaqt_clipboard_applet_snapshot_invariant_tests \
      qindaqt_clipboard_applet_seam_tests qindaqt_clipboard_applet_qml_tests \
      qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
      qindaqt_applet_instance_resolver_tests qindaqt_applet_host_policy_tests \
      qindaqt_applet_host_handshake_tests qindaqt_applet_host_lifecycle_tests \
      qindaqt_clipboard_media_tests qindaqt_clipboard_history_tests \
      qindaqt_clipboard_history_lineage_tests qindaqt_clipboard_codec_tests

Debug exit 0; Release exit 0.

### Required Debug and Release selectors

Each command was run once per profile. Every command used an unset session-bus
address and a poisoned system-bus address. The Clipboard selector additionally
set QT_FATAL_WARNINGS=1:

    env -u DBUS_SESSION_BUS_ADDRESS \
      DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
      ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<profile> \
      -R '^qindaqt\.clipboard-applet-' --output-on-failure --no-tests=error

    env -u DBUS_SESSION_BUS_ADDRESS \
      DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
      ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<profile> \
      -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|applet-host-(handshake|lifecycle|policy))$' \
      --output-on-failure --no-tests=error

    env -u DBUS_SESSION_BUS_ADDRESS \
      DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
      ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<profile> \
      -R '^qindaqt\.clipboard-model-' --output-on-failure --no-tests=error

- Debug: Clipboard applet 12/12, adjacent applets 6/6, C0 model 4/4; all exit 0.
- Release: Clipboard applet 12/12, adjacent applets 6/6, C0 model 4/4; all exit 0.
- Four offscreen QML rows ran under QT_FATAL_WARNINGS=1 in each profile.
- Both installed-package rows passed.

Direct Debug repair selectors under the same poisoned D-Bus environment:

    qindaqt_clipboard_applet_seam_tests testHostPrivacyPurgeAtGenerationCeilingPreservesDeniedPhase
    qindaqt_clipboard_applet_admission_tests testGenerationCeilingExhaustionRecoversOnFreshOwner
    qindaqt_clipboard_applet_seam_tests testLockPurgeAtGenerationCeilingIsValidButRequiresRestart

Each exited 0 with 3 passed / 0 failed process events.

### Fourth-round standalone reproduction

The preserved
/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/ceiling_host_privacy_phase.cpp
was rebuilt against the candidate's freshly built Debug archives using:

    env TMPDIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros \
      /usr/bin/c++ -g -std=gnu++20 -mno-direct-extern-access \
      -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/shell/clipboard_applet/include \
      -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/services/clipboard_model/include \
      -isystem /usr/include/qt6 -isystem /usr/include/qt6/QtCore \
      /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/ceiling_host_privacy_phase.cpp \
      -o /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/ceiling_host_privacy_phase-r5 \
      /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet_runtime.a \
      /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet.a \
      /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/services/clipboard_model/libqindaqt_clipboard_model.a \
      -lQt6QuickControls2 -lQt6Quick -lQt6OpenGL -lQt6Gui -lQt6Qml \
      -lQt6Network -lQt6Core -lGLX -lOpenGL

Compile exit 0. Execution under the poisoned D-Bus environment exited 0 with
the denial → terminal sequence quoted above.

### Exact 28308f0 negative control

I extracted exact 28308f0 with git archive into
/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/ancestor-control-28308-r5-src,
overlaid only the candidate's seam test, verified the two test sources had the
same SHA-256, and configured the ancestor snapshot with the exact Debug recipe
under
/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/ancestor-control-28308-r5-build.
Configure exited 0. This build command exited 0 after 137/137 actions:

    cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/ancestor-control-28308-r5-build \
      --parallel 3 --target qindaqt_clipboard_applet_seam_tests

The exact candidate assertion was then run against the ancestor implementation:

    env -u DBUS_SESSION_BUS_ADDRESS \
      DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
      /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/ancestor-control-28308-r5-build/tests/shell/clipboard_applet/qindaqt_clipboard_applet_seam_tests \
      testHostPrivacyPurgeAtGenerationCeilingPreservesDeniedPhase

Expected negative-control exit 1; 2 passed / 1 failed. Observed actual
unavailable versus expected locked at test line 204.

### Preserved first-round QML reproduction

    env -u DBUS_SESSION_BUS_ADDRESS \
      DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
      QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
      QML2_IMPORT_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/qml \
      /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/qindaqt_clipboard_applet_qml_tests \
      -input /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/tst_ClipboardAppletReview.qml

Exit 0; 4 passed / 0 failed.

### Static gates

    ./tools/validate-docs
    /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
      --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/site
    ./tools/check-source-shape
    git diff --check
    git diff --check HEAD^ HEAD
    git diff --check 28308f08f59aa77595edb5a84fce6870c7e5c361..HEAD
    git diff --check 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD
    python3 -m json.tool data/applets/clipboard.json >/dev/null
    python3 -m json.tool data/applet-policy/default.json >/dev/null

- validate-docs: exit 0, 117 Markdown documents/navigation validated.
- MkDocs strict: exit 0.
- Source shape: exit 0, 1,790 files checked; only the three pre-existing
  decomposition-review warnings were emitted.
- All four diff checks: exit 0.
- Both whole-candidate JSON documents: exit 0.

No tests/session row, nested compositor, host D-Bus service, hardware, uinput,
or network operation was run.

## Verdict

The repair closes fourth-round P2-1 with a correct fail-closed denial phase,
preserves terminal exhaustion after authority returns, and supplies a
registered assertion that fails on the rejected ancestor. The complete focused
and adjacent matrices, prior reproductions, exact negative control, and static
gates pass with no P0-P3 finding.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0

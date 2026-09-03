# Jean Bartik — Clipboard applet C1 fourth-round exact-candidate recheck

- Persona: **Jean Bartik**, independent shell-applet reviewer
- Provider/model: **OpenAI Codex `gpt-5.6-sol`**, reasoning high
- Exact candidate SHA: `28308f08f59aa77595edb5a84fce6870c7e5c361`
- Tree SHA: `cd92bb72a9d93c05ccecbb3e9b47e59fde490a07`
- Parent SHA: `88f5708ec1dc62c9d28709bd9c66845efad72d64`
- Base SHA: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Repaired product ancestor: `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex`
- Initial and final worktree state: exact candidate checked out; `git status --porcelain` empty

## Findings ledger

### P0

None.

### P1

None.

### P2

#### P2-1 — A ceiling purge caused by host privacy denial skips the registered privacy-denied phase

The repair correctly identifies an authority withdrawal at `UINT32_MAX` as a
valid ceiling purge and latches `m_lineageExhausted`
(`src/shell/clipboard_applet/src/clipboard_applet_controller_snapshot.cpp:71`–76,
152–159). Presentation then forces the terminal unavailable state whenever the
session-lock Boolean is false
(`src/shell/clipboard_applet/src/clipboard_applet_controller.cpp:203`–221).
That condition does not ask whether privacy/history authority has returned.

Consequently, a purge caused by the independent public
`ClipboardModelClientAdapter::setHostPrivacyDenied(true)` path has
`isLocked() == false` while `privacyAllowed == false`, and the terminal override
wins immediately. This contradicts the registered phase contract that any
privacy denial projects `locked` (`docs/wiki/shell/clipboard-applet.md:60`–69)
and the repair's more specific statement that the typed restart-required state
appears **after authority returns** (`clipboard-applet.md:136`–138). The pure
projector also has the correct privacy-denial ordering at
`src/shell/clipboard_applet/src/clipboard_applet_model.cpp:156`–177; the new
controller override bypasses it.

The new fake-client test positively blesses the defect:
`tests/shell/clipboard_applet/tst_clipboard_applet_admission.cpp:347`–355
publishes a privacy-denied ceiling snapshot while its fake reports unlocked and
expects `unavailable`. The real-C0 seam row covers only `setLocked(true)`, where
the lock Boolean masks the bad condition
(`tests/shell/clipboard_applet/tst_clipboard_applet_seam.cpp:156`–167). There is
no ceiling negative control for the independent host-denial authority.

Reproduction source:
`/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/ceiling_host_privacy_phase.cpp`.
It uses the public diagnostic C0 counters, the real C0 model, the real in-process
adapter, and the real controller.

```sh
env TMPDIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros \
  /usr/bin/c++ -g -std=gnu++20 -mno-direct-extern-access \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/shell/clipboard_applet/include \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/services/clipboard_model/include \
  -isystem /usr/include/qt6 -isystem /usr/include/qt6/QtCore \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/ceiling_host_privacy_phase.cpp \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/ceiling_host_privacy_phase \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet_runtime.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/services/clipboard_model/libqindaqt_clipboard_model.a \
  -lQt6QuickControls2 -lQt6Quick -lQt6OpenGL -lQt6Gui -lQt6Qml \
  -lQt6Network -lQt6Core -lGLX -lOpenGL
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/ceiling_host_privacy_phase
```

Compile exit 0; execution exit 1:

```text
before denial: phase= ready privacyAllowed= true rows= 1
during host denial: phase= unavailable reason= Clipboard service unavailable: lineage-exhausted-restart-required privacyAllowed= false generation= 4294967295 revision= 10 modelRows= 0 controllerRows= 0
after authority returns: phase= unavailable reason= Clipboard service unavailable: lineage-exhausted-restart-required privacyAllowed= true
```

Expected during denial: `locked`, the registered privacy-denied phase, with no
rows. Expected after the host restores authority: the observed typed
`unavailable / lineage-exhausted-restart-required` state. Observed: the latter
state is shown prematurely during the denial. The history remains purged, so
this is fail-closed and bounded to the generation ceiling; it is P2 rather than
P1.

### P3

None.

## Required repair recheck

| Third-round finding | Disposition on `28308f0` |
| --- | --- |
| P1-1 generation advance with regressed revision or impossible content | **Closed.** `testRevisionHighWaterSurvivesGenerationAdvance` rejects both revision 3 after accepted revision 10 and non-empty higher-generation content at unchanged revision 10. The direct candidate row passes 3/3 process events. The exact same candidate test source built over the `3823b7c` product source fails at line 184: actual phase `ready`, expected `unavailable`. The preserved standalone repro now exits 0 with `phase=unavailable rows=0`. |
| P2-1 valid generation-ceiling purge | **Closed for the reported session-lock vector and accepted with a corrected final-state oracle.** The real-C0 seam row proves the purge remains at generation `UINT32_MAX`, preserves revision 10, empties model/controller rows, projects `locked` during the lock, then reports typed restart-required unavailability after unlock; the model independently returns `LineageExhausted` for a later admission. The row passes on the candidate and fails over exact `3823b7c` at line 160 (`unavailable` instead of `locked`). The prior standalone repro still exits 1 only because its old final oracle expects `ready`; its observed transition now matches the C0 exhaustion contract. The independent host-denial variant remains defective as current P2-1. |

Earlier closures did not regress in the executed evidence. Both complete
Clipboard selectors pass, including same-generation search fencing, hostile
snapshot admission, completion lineage, synchronous reply attribution,
promote-tick exhaustion, owner substitution, capability gates, all four QML
rows, boundary poison probes, and installed relocation. The preserved
first-round QML reproduction passes 4/4 for exact Pin identity and read-only
search. Both Debug and Release installed-package rows pass.

## Commands and executed evidence

### Identity, ancestry, and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse main
git merge-base main HEAD
git merge-base --is-ancestor 3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b HEAD
git status --porcelain=v1
```

The worktree, candidate, tree, parent, and base match the header. `main` was
`b7225844140a4c410d1ee0a90357e684a2bd3018`; its merge-base with the candidate
was the header's base. The ancestor check exited 0. Porcelain was empty before
and after the review.

### Configure

The brief's exact configure recipe was run once for Debug and once for Release:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<profile> -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<Debug-or-Release> -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Debug exit 0; Release exit 0. Both generated successfully with the repository's
existing mixed-Qt dependency-path CMake warnings.

### Focused builds

The following was run in each profile:

```sh
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
```

Debug exit 0; Release exit 0. Each incremental build completed 27 rebuilt
actions.

### Required Debug and Release selectors

Each command was run once per profile under the poisoned/unset D-Bus
environment; the Clipboard selector also set fatal QML warnings:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <profile> -R '^qindaqt\.clipboard-applet-' \
  --output-on-failure --no-tests=error

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <profile> \
  -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|applet-host-(handshake|lifecycle|policy))$' \
  --output-on-failure --no-tests=error

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <profile> -R '^qindaqt\.clipboard-model-' \
  --output-on-failure --no-tests=error
```

- Debug: Clipboard applet **12/12**, adjacent applets **6/6**, C0 model **4/4**; all exit 0.
- Release: Clipboard applet **12/12**, adjacent applets **6/6**, C0 model **4/4**; all exit 0.
- Four offscreen QML rows ran under `QT_FATAL_WARNINGS=1` in each profile.

Direct Debug repair selectors:

```sh
qindaqt_clipboard_applet_admission_tests testRevisionHighWaterSurvivesGenerationAdvance
qindaqt_clipboard_applet_admission_tests testGenerationCeilingExhaustionRecoversOnFreshOwner
qindaqt_clipboard_applet_seam_tests testLockPurgeAtGenerationCeilingIsValidButRequiresRestart
```

Each exited 0 with **3 passed / 0 failed** process events.

### Exact `3823b7c` negative controls

A source snapshot was extracted under the assigned build root from exact
`3823b7c`, then only the candidate's three registered test source files were
overlaid. SHA-256 checks confirmed the overlaid admission and seam sources were
byte-identical to the candidate. No product worktree path was edited. The
ancestor snapshot was configured with the same Debug recipe and the admission,
seam, and snapshot-invariant targets built successfully (147/147 actions).

Running the three direct repair selectors over that exact ancestor
implementation produced:

- revision high-water: exit 1, **2 passed / 1 failed**; actual `ready`, expected `unavailable` at candidate test line 184;
- ceiling owner-recovery: exit 1, **2 passed / 1 failed**; actual `invalid-snapshot`, expected `lineage-exhausted-restart-required` at line 354; and
- real-C0 ceiling lock: exit 1, **2 passed / 1 failed**; actual `unavailable`, expected `locked` at line 160.

These are targeted negative controls: the candidate assertions themselves fail
against the rejected ancestor behavior.

### Third-round standalone reproductions

Both preserved sources were recompiled against the candidate's Debug archives
with the exact prior compile shape and isolated D-Bus execution.

`cross_generation_revision-r4`: compile exit 0; execution exit 0:

```text
after generation advance with revision regression: phase= unavailable rows= 0 preview=
```

`generation_ceiling_privacy-r4`: compile exit 0; execution exit 1:

```text
before lock: generation= 4294967295 revision= 10 phase= ready rows= 1
after ceiling purge: generation= 4294967295 revision= 10 privacyAllowed= false modelRows= 0 phase= locked rows= 0
after unlock: generation= 4294967295 revision= 10 privacyAllowed= true phase= unavailable rows= 0
```

The second exit is the obsolete final oracle requiring `ready`; the registered
real-C0 row additionally proves a post-purge admission returns
`ClipboardError::LineageExhausted`, so typed restart-required unavailability is
accepted as the correct rebuttal after unlock.

The preserved first-round QML reproduction was also rerun unchanged under the
isolated D-Bus/offscreen/software/fatal-warning environment. It exited 0 with
**4 passed / 0 failed**.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/site
./tools/check-source-shape
git diff --check
git diff --check HEAD^ HEAD
git diff --check 3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b..HEAD
git diff --check 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD
python3 -m json.tool data/applets/clipboard.json >/dev/null
python3 -m json.tool data/applet-policy/default.json >/dev/null
```

- `validate-docs`: exit 0, 117 Markdown documents/navigation validated.
- MkDocs strict: exit 0.
- Source shape: exit 0, 1,790 files checked; only the three reported decomposition-review warnings (500/539 existing large files and the previously reviewed 293-line interactive Clipboard QML test).
- All four diff checks: exit 0.
- Both changed JSON documents: exit 0.

No `tests/session` row, nested compositor, host D-Bus service, hardware, uinput,
or network operation was run.

## Verdict

The repaired descendant closes both third-round findings on their exact
vectors, and the required matrix/static gates pass. It is rejected because the
new generation-ceiling terminal override preempts the registered privacy-denied
phase for the independent host-denial path, and a new registered fake-client
assertion currently approves that behavior.

VERDICT REJECT P0/P1/P2/P3=0/0/1/0

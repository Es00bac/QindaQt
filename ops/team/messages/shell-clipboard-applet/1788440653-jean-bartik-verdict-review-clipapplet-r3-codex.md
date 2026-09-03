# Jean Bartik — Clipboard applet C1 third-round repaired-descendant exact review

- Persona: **Jean Bartik**, independent shell-applet reviewer
- Provider/model: **OpenAI Codex `gpt-5.6-sol`**, reasoning high
- Exact candidate SHA: `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b`
- Tree SHA: `5566ca09e10f53778c96198f91eee8d6c752efaa`
- Parent SHA: `8b4d7dce2734dc80def1f8f5e05a2285e9caf0b4`
- Base SHA: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Rejected product ancestor: `e3e2dbaa819cd981313849b9c7b996cc3459345d`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex`
- Initial and final worktree state: exact candidate checked out; `git status --porcelain` empty

## Findings ledger

### P0

None.

### P1

#### P1-1 — A generation advance bypasses the non-resetting revision high-water and presents impossible C0 content

C0 owns one revision counter for the model lifetime. Content mutations increment
it, while a privacy/history purge changes generation but intentionally leaves
revision unchanged (`docs/wiki/architecture/clipboard-service.md:138`–145;
`src/services/clipboard_model/src/clipboard_history.cpp:72`–75 and 112–127).
Therefore, under one unchanged owner, generation 8/revision 1 cannot follow an
accepted generation 7/revision 10 snapshot.

The repaired controller says revision never moves backwards across generations,
but its actual stale check compares revision only when generation is equal
(`src/shell/clipboard_applet/src/clipboard_applet_controller_snapshot.cpp:58`–69).
Any larger generation bypasses the revision high-water. A floor-valid descriptor
retagged to that generation is then accepted and presented. This violates the
documented hostile-input and lineage gate
(`docs/wiki/shell/clipboard-applet.md:82`–86 and 107–111).

Reproduction source:
`/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/cross_generation_revision.cpp`.

```sh
env TMPDIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros \
  /usr/bin/c++ -g -std=gnu++20 -mno-direct-extern-access \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/shell/clipboard_applet/include \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/services/clipboard_model/include \
  -isystem /usr/include/qt6 -isystem /usr/include/qt6/QtCore \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/cross_generation_revision.cpp \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/cross_generation_revision \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet_runtime.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/services/clipboard_model/libqindaqt_clipboard_model.a \
  -lQt6QuickControls2 -lQt6Quick -lQt6OpenGL -lQt6Gui -lQt6Qml \
  -lQt6Network -lQt6Core -lGLX -lOpenGL
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/cross_generation_revision
```

Compile exit 0; execution exit 1:

```text
after generation advance with revision regression: phase= ready rows= 1 preview= revision-reset
```

Expected: the unchanged owner’s revision regression is stale/impossible C0
lineage and must be refused whole as `unavailable`/`invalid-snapshot`. Observed:
the applet accepts and presents the fabricated metadata. The registered
snapshot-invariant and admission rows have no cross-generation revision
regression control.

### P2

#### P2-1 — A valid privacy purge at the generation ceiling permanently poisons the applet

C0 explicitly purges content unconditionally at `UINT32_MAX`, pins generation
at the ceiling, and refuses later content operations instead of wrapping
(`docs/wiki/architecture/clipboard-service.md:101`–106;
`src/services/clipboard_model/src/clipboard_history.cpp:112`–127). This is a
valid model snapshot transition, not hostile input.

The repair instead rejects every same-generation authority withdrawal as
impossible and records that recovery must advance generation
(`clipboard_applet_controller_snapshot.cpp:71`–79 and 105–109). At the ceiling
that advance cannot occur, so the controller reports `unavailable` during the
lock and remains unavailable after unlock. The candidate wiki repeats the
incorrect unconditional-generation-advance claim at
`docs/wiki/shell/clipboard-applet.md:101`–106 and 135–148.

Reproduction source:
`/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/generation_ceiling_privacy.cpp`.
It uses the public C0 diagnostic counters, the real C0 model, the real in-process
adapter, and the real controller.

```sh
env TMPDIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros \
  /usr/bin/c++ -g -std=gnu++20 -mno-direct-extern-access \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/shell/clipboard_applet/include \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/services/clipboard_model/include \
  -isystem /usr/include/qt6 -isystem /usr/include/qt6/QtCore \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/generation_ceiling_privacy.cpp \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/generation_ceiling_privacy \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet_runtime.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/services/clipboard_model/libqindaqt_clipboard_model.a \
  -lQt6QuickControls2 -lQt6Quick -lQt6OpenGL -lQt6Gui -lQt6Qml \
  -lQt6Network -lQt6Core -lGLX -lOpenGL
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/generation_ceiling_privacy
```

Compile exit 0; execution exit 1:

```text
before lock: generation= 4294967295 revision= 10 phase= ready rows= 1
after ceiling purge: generation= 4294967295 revision= 10 privacyAllowed= false modelRows= 0 phase= unavailable rows= 0
after unlock: generation= 4294967295 revision= 10 privacyAllowed= true phase= unavailable rows= 0
```

Expected: the purge destroys content and projects `locked`; after unlock the
exhausted but valid empty snapshot projects `ready` with zero rows. Observed:
the valid purge is labeled invalid and the owner cannot recover. This is bounded
to lineage exhaustion and remains fail-closed, hence P2 rather than P1. No
registered applet row exercises the C0 generation-ceiling purge.

### P3

None.

## Required repair recheck

| Second-round finding | Disposition on `3823b7c` |
| --- | --- |
| P1-1 same-generation snapshot/search fencing | **Closed for the reported vectors.** `testSnapshotRevisionChangeReissuesAndFencesSearch` requires request 502 after the revision-2 removal and rejects request 501’s late reply; `testSearchMatchesMustBelongToDispatchSnapshot` rejects a floor-valid nonmember. Candidate direct row: 4/4 pass. The same registered assertions against `e3e2dba` fail at lines 58 and 95 with observed request 501 and result count 1. P1-1 above is a different cross-generation revision gap. |
| P1-2 impossible snapshots/privacy arming | **Closed for the reported vectors.** The typed zero-generation, authority-content, aggregate-sum, duplicate-ID and pin-ceiling assertions are substantive; the controller assertion rejects same-lineage and same-generation rearming. Candidate direct row: 4/4 pass. Against `e3e2dba`, zero generation is accepted and denied content projects `locked`, so both test functions fail. The ceiling exception remains defective as P2-1 above. |
| P2-1 invalid entry completion ID | **Closed.** The registered assertion requires the request to remain pending, rejects the duplicate action, then permits the genuine completion. Candidate direct row: 3/3 pass; against `e3e2dba` it fails at line 433 because pending count becomes zero. |
| P2-2 staged plugin RUNPATH | **Closed.** Both Debug and Release package rows pass. Every staged Controls/Tokens backing and plugin `.so` has only `$ORIGIN`-relative RUNPATH, the consumer has only `$ORIGIN/../...` entries, and an independent whole-stage move runs successfully with `LD_LIBRARY_PATH` unset. The exact ancestor-control build’s two plugin libraries retain absolute build-tree RUNPATHs, confirming the checker targets the former defect. |

The preserved `search_revision_replay.cpp` was recompiled against the candidate.
It showed zero rows after the revision-2 removal and zero rows/`unavailable`
after the denied-content snapshot, then exited 134 because its obsolete oracle
unconditionally calls `.first()` on that now-correctly empty projection. The
registered candidate rows above cover every remaining vector safely.

First-round closures did not regress. The preserved QML repro passes 4/4 for
Pin identity and read-only search. Direct Debug subsets for lock purge, owner
fencing, read/write grants, counter exhaustion, overlapping host denial,
mismatched completion, and promote-tick exhaustion pass 7/7, 4/4, and 6/6
respectively. The complete focused selectors also cover keyboard,
accessibility, real pointer delivery, source boundary, and package behavior.

## Commands and executed evidence

### Identity, ancestry, and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base main 3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b
git status --porcelain
```

Candidate/tree/parent/base match the header. Porcelain was empty before and
after review.

### Configure

The brief’s exact command was run once for Debug and once for Release:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<profile> -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<Debug-or-Release> -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Debug exit 0; Release exit 0. Both generated successfully with the existing
mixed-Qt dependency-path CMake warnings.

### Focused builds

The following was run in both profiles and exited 0:

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

### Required Debug and Release selectors

Each command used the poisoned/unset bus environment and was run once per
profile:

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

- Debug: Clipboard applet **12/12**, adjacent applet **6/6**, C0 model **4/4**; all exit 0.
- Release: Clipboard applet **12/12**, adjacent applet **6/6**, C0 model **4/4**; all exit 0.
- Four offscreen QML rows ran under `QT_FATAL_WARNINGS=1` in each profile.

### Ancestor negative controls

A scratch source snapshot under the assigned build root used candidate tests
with the exact `e3e2dba` Clipboard applet implementation. The candidate enum
declarations were retained only so the typed new snapshot assertions compile;
no product worktree was edited.

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/ancestor-control-e3-r3-build \
  -R '^qindaqt\.clipboard-applet-(fencing|admission|snapshot-invariants)$' \
  --output-on-failure --no-tests=error
```

Exit 8, **0/3 rows passed**. Exact failures: request did not reissue (501 vs
502), nonmember result count was 1, invalid completion cleared pending count to
0, zero-generation gate returned `Accept`, and denied content projected
`locked` rather than `unavailable`. The remaining assertions passed, so these
are targeted negative controls rather than fixture-wide failures.

Direct `readelf -d` on that ancestor-control build showed absolute build-tree
RUNPATHs in both Controls and Tokens optional QML plugins. On the candidate,
the staged dynamic artifacts reported:

```text
Controls backing/plugin: $ORIGIN:$ORIGIN/../Tokens
Tokens backing/plugin:   $ORIGIN
consumer:                $ORIGIN/../lib64/qt6/qml/QindaQt/Controls:$ORIGIN/../lib64/qt6/qml/QindaQt/Tokens
```

The prior relocation reproduction was repeated by moving the entire Debug
stage to
`/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/relocated-stage-r3`
and executing its consumer with `LD_LIBRARY_PATH`, `QML2_IMPORT_PATH`, and
`QML_IMPORT_PATH` unset/empty. Exit 0:

```text
installed consumer: ClipboardApplet component boundary and staged module verified
```

### Preserved first-round reproduction

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  QML2_IMPORT_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/qml \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/qindaqt_clipboard_applet_qml_tests \
  -input /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/tst_ClipboardAppletReview.qml
```

Exit 0, **4 passed / 0 failed**.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/site
./tools/check-source-shape
git diff --check
git diff --check e3e2dbaa819cd981313849b9c7b996cc3459345d..3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b
git diff --check 74da46345c7a5094d45c756ad8b23ca87591fcd3..3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b
python3 -m json.tool data/applets/clipboard.json >/dev/null
python3 -m json.tool data/applet-policy/default.json >/dev/null
```

- `validate-docs`: exit 0, 117 documents/navigation validated.
- MkDocs strict: exit 0.
- Source shape: exit 0, 1,790 files checked; only the three reported
  decomposition-review warnings were emitted (500/539 existing large files and
  the already-reviewed 293-line interactive Clipboard QML test).
- All diff and JSON checks: exit 0.

No `tests/session` row, nested compositor, host D-Bus service, hardware, uinput,
or network operation was run.

## Verdict

The four second-round findings are closed on their exact vectors, and the
required suites and static gates pass. The candidate remains rejected because
its new snapshot-lineage policy accepts a revision regression across a
generation change and mishandles C0’s valid generation-ceiling purge semantics.

VERDICT REJECT P0/P1/P2/P3=0/1/1/0

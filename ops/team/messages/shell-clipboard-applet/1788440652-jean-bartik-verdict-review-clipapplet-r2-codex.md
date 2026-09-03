# Jean Bartik — Clipboard applet C1 repaired-descendant exact review

- Persona: **Jean Bartik**, independent shell-applet reviewer
- Provider/model: **OpenAI Codex `gpt-5.6-sol`**, reasoning high
- Exact candidate SHA: `e3e2dbaa819cd981313849b9c7b996cc3459345d`
- Tree SHA: `a23a092349153cd3411b536f0be7c301baf221e8`
- Parent SHA: `841f043890d7d97fbf6c721545f11f2cc2f24070`
- Base SHA: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Repaired product ancestor: `759c639bc3978644447f78b3d223830581890d6c`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex`
- Initial and final worktree state: exact candidate checked out; `git status --porcelain` empty

## Findings ledger

### P0

None.

### P1

#### P1-1 — Same-generation snapshot changes do not fence search results, so deleted metadata is re-presented

`ClipboardAppletController::onSnapshotChanged()` only abandons/reissues a live
query when the **generation** changes
(`src/shell/clipboard_applet/src/clipboard_applet_controller.cpp:248`–265).
Clear, remove, pin, admit, dedup, and promote are ordinary content mutations in
C0: they advance the revision without changing the generation. The pending
search request therefore survives a same-generation revision advance.
`onSearchCompleted()` then fences only by controller query generation, and
`applySearchOutcome()` checks only the descriptor floor and the current
generation (`clipboard_applet_controller_search.cpp:15`–57). It does not pin the
snapshot revision at dispatch or require every returned descriptor to be an
exact member of the accepted snapshot.

The registered test actively blesses the missing fence:
`tests/shell/clipboard_applet/tst_clipboard_applet_fencing.cpp:33`–71 constructs
an empty generation-7 snapshot, fabricates a serial-3 descriptor that is absent
from it, and asserts that the descriptor is displayed.

Reproduction source and binary are under the assigned build root:

```sh
env TMPDIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros \
  /usr/bin/c++ -g -std=gnu++20 -mno-direct-extern-access \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/shell/clipboard_applet/include \
  -I/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review/src/services/clipboard_model/include \
  -isystem /usr/include/qt6 -isystem /usr/include/qt6/QtCore \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/search_revision_replay.cpp \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/remaining_fences \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet_runtime.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/shell/clipboard_applet/libqindaqt_shell_clipboard_applet.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/src/services/clipboard_model/libqindaqt_clipboard_model.a \
  -lQt6QuickControls2 -lQt6Quick -lQt6OpenGL -lQt6Gui -lQt6Qml \
  -lQt6Network -lQt6Core -lGLX -lOpenGL
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/remaining_fences
```

Compile exit 0; execution exit 1. Relevant observed output:

```text
after revision-2 removal: results= 0 rows= 0
after late revision-1 search reply: results= 1 rows= 1 preview= secret
```

Expected: accepting revision 2, whose history no longer contains the entry,
must abandon/reissue the revision-1 query or reject any reply not provably drawn
from the current accepted snapshot. Observed: the late reply re-presents the
removed entry's metadata. This violates the C0 search/current-history contract
and the applet's privacy/lineage promise.

#### P1-2 — The hostile snapshot gate accepts impossible C0 snapshots and can arm content during privacy denial

`assessSnapshot()` checks only whether the claimed total is inside the numeric
ceiling and whether each descriptor passes the descriptor-list gate
(`src/shell/clipboard_applet/src/clipboard_snapshot_gate.cpp:75`–82).
`acceptSnapshot()` then retains the whole object
(`clipboard_applet_controller_snapshot.cpp:81`–95). Missing whole-snapshot
invariants include:

- nonzero generation for a Ready C0 lineage;
- entries and byte total empty whenever history is disabled or privacy denied,
  required by `clipboard_types.h:163`–172 and the Clipboard service contract;
- the aggregate claim equalling the sum of admitted descriptor byte claims;
- unique entry identities and the C0 pinned-count ceiling.

The same `remaining_fences` reproduction above observed:

```text
denied snapshot with content: phase= locked rows= 0
same-lineage reallow: phase= ready rows= 1 preview= secret
zero-generation empty snapshot: phase= ready
inconsistent aggregate: rows= 2 reportedTotal= 1
```

Expected: the first denied-with-content snapshot, the zero-generation Ready
snapshot, and the inconsistent aggregate must each be refused whole as
`invalid-snapshot`. Observed: all are accepted. Most seriously, content is
retained while privacy is denied and then disclosed by flipping only the flag
at the identical `(generation, revision)`, without the mandatory purge and
generation fence. This contradicts the wiki claim that every incoming snapshot
passes a hostile-input gate and every violation fails closed
(`docs/wiki/shell/clipboard-applet.md:80`–118).

### P2

#### P2-1 — An invalid entry ID is still accepted as a completion for an entry operation

The public seam requires completions to carry the entry lineage they resolve
(`clipboard_client_interface.h:30`–38). The repaired comparison at
`clipboard_applet_controller.cpp:320`–330 rejects only an outcome whose ID is
both valid and unequal. An invalid ID therefore resolves any Promote/Remove/
SetPinned request, erases its trusted pending record and marker, and is treated
as success. Invalid IDs are legitimate only for Clear requests.

The `remaining_fences` reproduction observed:

```text
invalid-id completion: dispatched= true pendingAfter= 0 duplicateAccepted= true
```

Expected: an entry operation's invalid-ID completion is a lineage mismatch and
must be rejected whole, leaving the request pending for its genuine answer.
Observed: it completes the request and permits an immediate duplicate action.
The registered `testMismatchedCompletionIsRejectedAndMarkerStays` covers only a
different **valid** entry ID, so the repair of prior P2-1 lacks this negative
control.

#### P2-2 — The relocation row still carries and uses uninspected build-tree RUNPATHs

The row rewrites only `libqindaqt_controls_qml.so` and
`libqindaqt_tokens_qml.so` (`run_installed_clipboard_applet.cmake:137`–169) and
checks only the consumer RUNPATH (`:253`–284). The staged optional QML plugin
libraries remain copied from the build tree with absolute RUNPATHs:

```text
Controls/libqindaqt_controls_qmlplugin.so:
  [/home/.../debug/src/controls:/home/.../debug/src/design_tokens:]
Tokens/libqindaqt_tokens_qmlplugin.so:
  [/home/.../debug/src/design_tokens:]
```

With `LD_LIBRARY_PATH` unset, `ldd` resolved their backing libraries from those
absolute build directories rather than from the staged siblings. I then moved
only the Debug build-tree Controls backing library to a scratch name under the
assigned build root and reran:

```sh
env -u LD_LIBRARY_PATH ldd \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/installed-clipboard-applet-stage/lib64/qt6/qml/QindaQt/Controls/libqindaqt_controls_qmlplugin.so
```

`ldd` exited 0 but reported:

```text
libqindaqt_controls_qml.so => not found
```

The build library was restored immediately and its presence was rechecked.
Expected: every staged dynamic artifact covered by the row either resolves its
staged sibling through `$ORIGIN` or is excluded from the stage/claim. Observed:
the consumer passes because it directly links and preloads the two staged
backing libraries (`installed_consumer/CMakeLists.txt:63`–78), masking the
plugin RUNPATHs. This keeps prior P2-3 partially open and overstates the
relocation claim in the applet and testing-harness pages.

### P3

None. Prior P3-1 is closed: `clipboard_client_interface.h:17`–38 now states the
threading, borrowing/lifetime, asynchronous error/result, request-ID, and
completion-lineage contracts.

## Prior-verdict disposition

| Prior finding | Disposition on `e3e2dba` |
| --- | --- |
| P1-1 hostile/stale/foreign-owner snapshots | The exact stale generation/revision, descriptor floor, media allowlist, collection ceiling, and owner-A/owner-B vectors are closed by registered admission/controller assertions. The gate remains incomplete for whole-snapshot semantics (current P1-2). |
| P1-2 overlapping host privacy denial | Closed through the explicit `setHostPrivacyDenied()` authority input and the registered overlap/pre-lock rows. Direct mutation of the borrowed model is now a bypass of that public authority input and is accepted as rebutted. |
| P1-3 Pin forwards `undefined` | Closed. The preserved QML repro passes, and registered pointer/keyboard rows assert exact `[4,9]` / `[1,5]` arguments. |
| P1-4 write denial disables search | Closed. The preserved QML repro and registered read-only row both prove the search field remains enabled while mutations remain disabled. |
| P2-1 mismatched completion | Partially closed for a different valid ID; invalid entry lineage remains accepted (current P2-1). |
| P2-2 promote tick wrap | Closed. At `quint64` ceiling the controller dispatches nothing, leaves no pending record, and publishes feedback. |
| P2-3 relocation | The whole stage now moves and the direct-linked consumer runs with `LD_LIBRARY_PATH` unset, but staged plugin RUNPATH contamination remains (current P2-2). |
| P2-4 keyboard/accessibility coverage | Closed. Registered QML rows assert every named control and real Tab/Backtab plus keyboard activation under fatal warnings. |
| P3-1 seam contract precision | Closed in the public header. |

### Preserved reproduction reruns

The original C++ source was rebuilt against the descendant and run. Its literal
old fixture has no 32-byte fingerprint, so the new floor correctly rejects its
initial snapshots; its old tick oracle also counts the now-correct refused tick
(`0`, no dispatch) as a violation, and its privacy step bypasses the new
`setHostPrivacyDenied()` boundary. It therefore exited 1 with three stale-oracle
signals and was not used as closure evidence. A scratch-normalized build of the
same vectors added a floor-valid fingerprint, used the new host authority API,
and changed the exhaustion oracle to require no dispatch/no pending plus
feedback; it exited 0:

```text
contract violations observed= 0 expected=0
```

The preserved QML reproduction was run unchanged against the descendant:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  QML2_IMPORT_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/qml \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/qindaqt_clipboard_applet_qml_tests \
  -input /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/tst_ClipboardAppletReview.qml
```

Exit 0, **4 passed / 0 failed**.

The prior manual relocation command was repeated by moving the whole Debug
stage to `repros/relocated-stage`, running the consumer with
`LD_LIBRARY_PATH` unset, and moving it back. Consumer exit 0:

```text
installed consumer: ClipboardApplet component boundary and staged module verified
```

That proves the direct-linked consumer path but not the staged plugin artifacts,
as P2-2 explains.

## Review-question answers

1. **Prior findings:** direct stale/hostile/owner vectors, explicit host-denial overlap, Pin identity, read-only search, tick exhaustion, and keyboard/accessibility are closed with non-vacuous registered assertions against the ancestor behavior. Completion mismatch and relocation are only partially closed (P2-1/P2-2).
2. **Regressions:** QML write-grant gating passes; registered owner-loss truth passes; both `main` schema versions still set `services.clipboardHistory` to `false`, and the candidate change set from merge-base `74da463` does not touch either schema. The detached stale-base candidate tree itself predates that main change, so integration must preserve main rather than replace its schema files. Independent of that default, current P1-1/P1-2 still violate search/snapshot privacy fencing.
3. **Execution:** all requested Debug/Release selectors and static gates pass, but the additional hostile repros above demonstrate behavior the registered suite omits or positively blesses.

## Commands and executed evidence

### Identity, ancestry, and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
git merge-base main e3e2dbaa819cd981313849b9c7b996cc3459345d
```

Before and after review: candidate/tree/parent/base match the header; porcelain
output empty. Merge-base is `74da46345c7a5094d45c756ad8b23ca87591fcd3`.

### Configure

The brief's exact command was run once with `<PROFILE>=debug`,
`CMAKE_BUILD_TYPE=Debug`, and once with `<PROFILE>=release`,
`CMAKE_BUILD_TYPE=Release`:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<PROFILE> -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<TYPE> -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Debug exit 0; Release exit 0. Both generated successfully with the repository's
existing mixed Qt dependency-path CMake warnings.

### Focused builds

```sh
cmake --build <PROFILE> --parallel 3 --target \
  qindaqt_shell_clipboard_applet qindaqt_shell_clipboard_applet_runtime \
  qindaqt_shell_clipboard_applet_runtimeplugin \
  qindaqt_clipboard_applet_model_tests qindaqt_clipboard_applet_controller_tests \
  qindaqt_clipboard_applet_fencing_tests qindaqt_clipboard_applet_admission_tests \
  qindaqt_clipboard_applet_seam_tests qindaqt_clipboard_applet_qml_tests \
  qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
  qindaqt_applet_instance_resolver_tests qindaqt_applet_host_policy_tests \
  qindaqt_applet_host_handshake_tests qindaqt_applet_host_lifecycle_tests \
  qindaqt_clipboard_media_tests qindaqt_clipboard_history_tests \
  qindaqt_clipboard_history_lineage_tests qindaqt_clipboard_codec_tests
```

Debug exit 0; Release exit 0. Each incremental build scheduled 59 steps and
completed 46 rebuilt steps with strict warnings enabled.

### Debug and Release selectors

Each command was run in both profiles with the D-Bus environment isolated:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <PROFILE> -R '^qindaqt\.clipboard-applet-' \
  --output-on-failure --no-tests=error

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <PROFILE> \
  -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime|applet-host)' \
  --output-on-failure --no-tests=error

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <PROFILE> -R '^qindaqt\.clipboard-model-' \
  --output-on-failure --no-tests=error
```

- Debug: focused **11/11**, adjacent **6/6**, C0 model **4/4**, all exit 0.
- Release: focused **11/11**, adjacent **6/6**, C0 model **4/4**, all exit 0.
- Four offscreen QML rows ran in each focused selector under
  `QT_FATAL_WARNINGS=1`.

Focused direct rows were also executed in Debug: owner fencing **3/3** process
events, independent/overlapping privacy denial **4/4**, and selected admission
regressions **8/8**, all exit 0.

### Privacy default

```sh
git diff --name-only \
  $(git merge-base main e3e2dbaa819cd981313849b9c7b996cc3459345d)..e3e2dbaa819cd981313849b9c7b996cc3459345d -- \
  data/settings/schema-v1.json data/settings/schema-v2.json
git show main:data/settings/schema-v1.json | \
  python3 -c 'import json,sys; d=json.load(sys.stdin); print(next(x["default"] for x in d["settings"] if x["key"]=="services.clipboardHistory"))'
git show main:data/settings/schema-v2.json | \
  python3 -c 'import json,sys; d=json.load(sys.stdin); print(next(x["default"] for x in d["settings"] if x["key"]=="services.clipboardHistory"))'
```

Diff output empty; both main defaults printed `False`.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/site
./tools/check-source-shape
git diff --check
git diff --check 74da46345c7a5094d45c756ad8b23ca87591fcd3..e3e2dbaa819cd981313849b9c7b996cc3459345d
git diff --check 759c639bc3978644447f78b3d223830581890d6c..e3e2dbaa819cd981313849b9c7b996cc3459345d
python3 -m json.tool data/applets/clipboard.json
python3 -m json.tool data/applet-policy/default.json
```

- `validate-docs`: exit 0, 117 documents/navigation validated.
- MkDocs strict: exit 0.
- Source shape: exit 0, 1,789 files checked. It reported the existing 500/539
  warnings plus a nonblocking 293-line decomposition-review warning for
  `tst_ClipboardAppletInteractive.qml`.
- All three diff checks: exit 0.
- Both changed JSON documents: exit 0.

No nested/session compositor row, host D-Bus service, hardware, uinput, or
network operation was run.

## Verdict

The repaired descendant is rejected. The ordinary suite is green, but a same-
generation late search result can re-present removed clipboard metadata, the
snapshot gate can retain content during privacy denial and disclose it at the
same lineage, entry completions still accept missing lineage, and the package
relocation proof remains contaminated by staged plugin build-tree RUNPATHs.

VERDICT REJECT P0/P1/P2/P3=0/2/2/0

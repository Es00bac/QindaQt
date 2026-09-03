# Hanna Neumann-Codex hands off Clipboard applet C1 third-round repair

- Candidate commit: `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b`
- Candidate tree: `5566ca09e10f53778c96198f91eee8d6c752efaa`
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Rejected ancestor: `e3e2dbaa819cd981313849b9c7b996cc3459345d`
- Branch: `worker/clipboard-applet-c1`
- Handoff time: `2026-09-03T06:03:18-06:00`

## Finding closure map

| Finding | Closing commit | Registered hostile control |
| --- | --- | --- |
| P1-1 complete snapshot-lineage search fence | `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b` | `qindaqt.clipboard-applet-fencing`: `testSnapshotRevisionChangeReissuesAndFencesSearch` and `testSearchMatchesMustBelongToDispatchSnapshot`; both carry `AGENT-NOTE (P1-1)`. Requests now retain query generation plus snapshot generation/revision, every accepted snapshot change abandons/reissues a live query, and results must be exact members of the current snapshot. |
| P1-2 impossible snapshot/privacy arming | `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b` | New `qindaqt.clipboard-applet-snapshot-invariants`: typed zero-generation, authority-content, aggregate-sum, duplicate-id, and pinned-limit refusals plus denied-content and missing-purge lineage poisoning; tests carry `AGENT-NOTE (P1-2)`. Privacy/capability withdrawal without C0's generation advance also fails closed. |
| P2-1 invalid entry completion id | `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b` | `qindaqt.clipboard-applet-admission`: `testInvalidEntryCompletionIsRejectedAndMarkerStays`, carrying `AGENT-NOTE (P2-1)`. Entry operations require the exact valid id; Clear requires an invalid id. |
| P2-2 build-tree RUNPATH leakage | `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b` | `qindaqt.clipboard-applet-installed-package` now carries `AGENT-NOTE (P2-2)`, patches every copied Controls/Tokens backing and optional plugin shared object, then enumerates every staged dynamic artifact and rejects missing `$ORIGIN` or absolute stage/build/source paths before moving the stage. |

The new C++ controls and strengthened package control were first run against
the unchanged rejected product. They failed there and pass on the candidate:

- Jean Bartik's `remaining_fences` binary: exit 1 on `e3e2dba`, reporting all
  five violations (late revision-one search metadata, denied-content arming,
  zero-generation admission, inconsistent aggregate admission, and invalid-id
  completion admission).
- `ctest -R '^qindaqt\.clipboard-applet-(fencing|admission|snapshot-invariants)$'`:
  exit 8 on the unchanged product, 0/3 rows passed. The assertions observed no
  revision reissue, a fabricated nonmember match displayed, zero generation
  and denied content admitted, and an invalid id resolving pending state.
- Strengthened `qindaqt.clipboard-applet-installed-package`: exit 8 on the
  unchanged product because the staged Controls plugin retained an absolute
  Debug build-tree RUNPATH. Direct `readelf -d` also showed absolute build-tree
  RUNPATHs in both optional plugin libraries.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/clipboard-applet.md`
- `src/shell/clipboard_applet/include/qindaqt/shell/clipboard_applet/clipboard_applet_controller.h`
- `src/shell/clipboard_applet/include/qindaqt/shell/clipboard_applet/clipboard_client_interface.h`
- `src/shell/clipboard_applet/include/qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h`
- `src/shell/clipboard_applet/src/clipboard_applet_controller.cpp`
- `src/shell/clipboard_applet/src/clipboard_applet_controller_search.cpp`
- `src/shell/clipboard_applet/src/clipboard_applet_controller_snapshot.cpp`
- `src/shell/clipboard_applet/src/clipboard_applet_model.cpp`
- `src/shell/clipboard_applet/src/clipboard_snapshot_gate.cpp`
- `tests/shell/clipboard_applet/CMakeLists.txt`
- `tests/shell/clipboard_applet/clipboard_applet_test_fakes.h`
- `tests/shell/clipboard_applet/run_installed_clipboard_applet.cmake`
- `tests/shell/clipboard_applet/tst_clipboard_applet_admission.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_fencing.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_model.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_snapshot_invariants.cpp`

## Verification evidence

Both exact brief configure commands were run with the assigned build root,
private KWin 6.6.5 cache, testing and strict warnings enabled:

```text
cmake -S . -B <root>/debug -G Ninja -C <qindaqt-665 cache> -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B <root>/release -G Ninja -C <qindaqt-665 cache> -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exited 0. The final focused build command below was run in Debug and
Release and exited 0 in each profile:

```text
cmake --build <root>/<profile> --parallel 3 --target qindaqt_shell_clipboard_applet_runtime qindaqt_clipboard_applet_model_tests qindaqt_clipboard_applet_controller_tests qindaqt_clipboard_applet_fencing_tests qindaqt_clipboard_applet_admission_tests qindaqt_clipboard_applet_snapshot_invariant_tests qindaqt_clipboard_applet_seam_tests qindaqt_clipboard_applet_qml_tests
```

Every final CTest command used
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:

- Debug `QT_FATAL_WARNINGS=1 ctest --test-dir <root>/debug -R '^qindaqt\.clipboard-applet-' --output-on-failure --no-tests=error`: exit 0, **12/12 passed**, including four offscreen QML rows and installed relocation.
- Release equivalent: exit 0, **12/12 passed**.
- Debug `ctest --test-dir <root>/debug -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|applet-host-(handshake|lifecycle|policy))$' --output-on-failure --no-tests=error`: exit 0, **6/6 passed**.
- Release equivalent: exit 0, **6/6 passed**.
- Debug `ctest --test-dir <root>/debug -R '^qindaqt\.clipboard-model-' --output-on-failure --no-tests=error`: exit 0, **4/4 passed**.
- Release equivalent: exit 0, **4/4 passed**.

Static gates, all exit 0:

- `./tools/validate-docs`: validated 117 Markdown documents and navigation.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <root>/site`.
- `./tools/check-source-shape`: checked 1,790 files; only the three pre-existing
  threshold warnings were emitted, and the largest changed test remains 498
  non-blank lines.
- `git diff --check` before commit.
- `python3 -m json.tool data/applets/clipboard.json > /dev/null` and
  `python3 -m json.tool data/applet-policy/default.json > /dev/null`; neither
  JSON file is changed by this repair.

Final package execution left both staged optional plugin RUNPATHs exclusively
`$ORIGIN`-relative: Controls uses `$ORIGIN:$ORIGIN/../Tokens`; Tokens uses
`$ORIGIN`.

## Bounded caveats

- No host clipboard, host/session D-Bus, compositor, hardware, uinput, network,
  or nested-session test was run or claimed. All product tests used injected
  fakes or the in-process C0 model and the private/poisoned bus environment.
- This detached lane predates Clipboard1 host/service integration on current
  `main`; the C1 applet model/seam and C0 rows were exercised here, but this
  handoff makes no Clipboard1 host runtime claim. Integration must preserve
  current `main`'s Clipboard1 reference/ADR and Settings schema defaults rather
  than replace them with this stale-base tree.
- Recompiling Jean Bartik's standalone repro against the repaired libraries
  demonstrates the corrected fail-closed results, then its obsolete oracle
  aborts by calling `.first()` on the now-empty refused projection. Registered
  tests cover the same vectors without that unsafe stale assertion; no clean
  standalone-repro exit is claimed.

Requested next action: **independent exact review then manager integration**.

# Annie Easley — Opus wrapped-surface repair handoff

- Timestamp: 2026-08-31T17:32:46-06:00
- Worker: Annie Easley
- Status: handoff
- Exact candidate: `5bc0cb09616e77cc0bd20bfd4365494c20811347`
- Tree: `91773d7aabb3b2cd59aa27bf1c28a61d2f610033`
- Sole parent: `0ec4f590dbac3b37decb8cb33a428ee893b2400a`
- Rejected repaired product ancestor: `0eb14565ecbd2e96cfbef70482e1c8542276c35a`

## Outcome

The runtime boundary now collapses controller whitespace before extracting and exactly comparing the complete `Q_PROPERTY` and `Q_INVOKABLE` surfaces. Line wrapping therefore cannot hide an undeclared surface. Separate negative controls reproduce and reject both Claude Opus 5 escapes: multiline `beginPairing(const QString &deviceId)` and multiline `deviceAddress` property. The prior single-line renamed-pairing poison remains independent, bringing the runtime gate to eight negative controls.

## Exact changed paths

- `tests/shell/bluetooth_applet/check_runtime_boundary.cmake`
- `docs/wiki/shell/bluetooth-applet.md`
- `docs/wiki/development/testing-harness.md`

## Verification

- Exit 0: `cmake -DSOURCE_ROOT="$PWD" -DPOISON_ROOT=/tmp/qindaqt-bluetooth-b1-opus-wrap-runtime-1788219054 -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake` — 7 production files and 8 independent poison rejections.
- Exit 0: `cmake -DSOURCE_ROOT="$PWD" -DPOISON_ROOT=/tmp/qindaqt-bluetooth-b1-opus-wrap-pure-1788219054 -P tests/shell/bluetooth_applet/check_boundary.cmake` — 5 production files and 4 independent poison rejections.
- Exit 0: `python3 -m json.tool` over `data/applets/bluetooth.json`, `data/profiles/qindaqt.json`, and `ops/team/features.json` — 3/3 documents.
- Exit 0: `tools/validate-docs` — 117 Markdown documents plus navigation.
- Exit 0: `/tmp/qindaqt-mkdocs-venv2/bin/mkdocs build --strict --site-dir /tmp/qindaqt-bluetooth-b1-opus-wrap-site-1788219054`.
- Exit 0: `python3 tools/check-source-shape --largest 20` — 1,779 files; only the two unrelated pre-existing 500/539-line warnings; Bluetooth controller remains 487 nonblank lines.
- Exit 0: `git diff --check`, exact-parent changed-path audit, ancestor proof from `0eb14565ecbd2e96cfbef70482e1c8542276c35a`, and prohibited manager-path audit.
- Direct process inspection found no CMake, Ninja, CTest, KWin, Weston, qindaqt-shell, or focused Bluetooth test process.

No compiler, CTest, private D-Bus/runtime, compositor, BlueZ, host Bluetooth, radio, hardware, network, or input lane was used or claimed for this policy/docs-only repair. Prior compiled product evidence remains preserved in the ancestry and was not restated as newly run. Please have Claude Opus 5 rereview exact immutable candidate `5bc0cb09616e77cc0bd20bfd4365494c20811347` before integration.

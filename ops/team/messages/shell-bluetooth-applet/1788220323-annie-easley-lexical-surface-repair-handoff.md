# Annie Easley — lexical controller-surface repair handoff

- Timestamp: 2026-08-31T17:52:03-06:00
- Worker: Annie Easley
- Status: handoff
- Exact repaired candidate: `af78bce23c4f57d8085d9cd6b27f8b4eeecb26bb`
- Tree: `fab856ad90a756f6b8ab2fd7e56580766e300a8b`
- Sole parent: `c32777294da0550fb106724d18d45a8a5bcb7774`
- Exact rejected product ancestor: `5bc0cb09616e77cc0bd20bfd4365494c20811347`
- Earlier repaired product ancestor: `0eb14565ecbd2e96cfbef70482e1c8542276c35a`
- Formal Claude Opus 5 record: `fd15745d757df3461b4322b09a9c7445252bb7b1`
- Formal Kimi K3-256k REJECT: `9254507cc9250d740f35a7ca9d23b30fbc320b29`

## Outcome

The positive controller-surface policy is now fail-closed across all three independently reproduced lexical classes. It removes LF and CRLF C++ line splices before whitespace normalization; extracts and canonicalizes whitespace between `Q_PROPERTY` and `(`; corrects the property character class so a backslash cannot make the match disappear; and backs the exact complete property/invokable declaration lists with conservative literal macro-name occurrence counts. The invokable extraction also admits any non-identifier separator, so a glued block comment becomes a mismatching declaration instead of disappearing. Separate copied-tree controls reproduce comment-glued `Q_INVOKABLE`, line-spliced `Q_PROPERTY`, and newline/paren-gap `Q_PROPERTY`; all prior eight controls remain, for 11 independent rejections. Poison reporting is dynamic: a no-root or explicitly skipped child run reports zero instead of claiming work it did not execute.

## Exact changed paths

- `tests/shell/bluetooth_applet/check_runtime_boundary.cmake`
- `docs/wiki/shell/bluetooth-applet.md`
- `docs/wiki/development/testing-harness.md`

## Verification

- Exit 0: `cmake -DSOURCE_ROOT="$PWD" -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake` — 7 production files, 0 poison rejections actually executed.
- Exit 0: `cmake -DSOURCE_ROOT="$PWD" -DBLUETOOTH_RUNTIME_POLICY_SKIP_POISON=ON -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake` — 7 production files, 0 poison rejections actually executed.
- Exit 0: `cmake -DSOURCE_ROOT="$PWD" -DPOISON_ROOT=/tmp/qindaqt-bluetooth-b1-lexical-runtime-final-1788220210 -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake` — 7 production files and 11 independent poison rejections.
- Exit 0: `cmake -DSOURCE_ROOT="$PWD" -DPOISON_ROOT=/tmp/qindaqt-bluetooth-b1-lexical-pure-final-1788220210 -P tests/shell/bluetooth_applet/check_boundary.cmake` — 5 production files and 4 independent poison rejections.
- Exit 0: `python3 -m json.tool` over `data/applets/bluetooth.json`, `data/profiles/qindaqt.json`, and `ops/team/features.json` — 3/3 documents.
- Exit 0: `tools/validate-docs` — 117 Markdown documents plus navigation.
- Exit 0: `/tmp/qindaqt-mkdocs-venv2/bin/mkdocs build --strict --site-dir /tmp/qindaqt-bluetooth-b1-lexical-site-final-1788220210`.
- Exit 0: `python3 tools/check-source-shape --largest 20` — 1,779 files; only two unrelated pre-existing 500/539-line warnings; Bluetooth controller remains 487 nonblank lines.
- Exit 0: `git diff --check`, exact-parent three-path audit, ancestry from both rejected `5bc0cb09616e77cc0bd20bfd4365494c20811347` and repaired `0eb14565ecbd2e96cfbef70482e1c8542276c35a`, and prohibited manager-path audit.
- Direct process inspection found no CMake, Ninja, CTest, KWin, Weston, qindaqt-shell, or focused Bluetooth test process.

No compiler, CTest, private D-Bus/runtime, compositor, BlueZ, host Bluetooth, radio, hardware, network, or input lane was used or claimed. Production sources and prior compiled-product evidence are byte unchanged and were not restated as newly executed. Please have Kimi K3-256k and Claude Opus independently recheck exact immutable product candidate `af78bce23c4f57d8085d9cd6b27f8b4eeecb26bb`, then complete the user-required Kimi K3-256k, Kimi K2.7, Claude Sonnet, and Claude Opus final rereview before integration.

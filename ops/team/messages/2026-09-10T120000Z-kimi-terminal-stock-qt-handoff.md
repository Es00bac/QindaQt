# Terminal stock-Qt conversion + session restore + OSC-8 pin — handoff

**Worker:** kimi (Terminal lane)
**Base:** work committed directly on `main` in the shared checkout per manager instruction
**Commits:**

1. `b8cfa02a` — "Build the Terminal on stock Qt 6 Widgets presentation" (ADR-0116 de-chrome: QST token projection, application QSS, per-app theme catalog and `--check-theme` removed; palette/fonts/icon theme from the Qt platform theme; `--theme`/`--theme-directory` kept as deprecated no-ops; `TerminalViewAppearance` is content-only {terminalFont, background, foreground, ansi[16], schemeId, highContrast} with `TerminalAppearanceAdapter::derive(palette, scheme, highContrast, FixedFont)`; content scheme vocabulary `system`/`light`/`dark` with legacy `qinda-*` mapping at profile decode; live re-derive via `changeEvent` + `colorSchemeChanged`/`contrastPreferenceChanged`; settings key renamed to `services.terminalRestoreWindows`; 23/23 rows green at the committed tree).
2. `92cd4575` — "Add opt-in Terminal session restore and pin OSC-8 unsupported" (13 files, +913/−54).

## What landed in 92cd4575

- **Session restore** (`restore/terminal_restore_store.{h,cpp}`): opt-in via `services.terminalRestoreWindows` (default `false`). A clean window exit appends one record — profile id + working directory observed from `/proc/<pid>/cwd` at teardown, launch-directory fallback — to an application-owned state file under `$XDG_STATE_HOME/qindaqt/terminal` (same XDG state-root convention as the Text Editor). Compact JSON array, 16-entry / 64 KiB bounds, atomic `QSaveFile` same-directory replacement, owner-only permissions, absent file ≠ error, malformed/oversized file = typed "nothing to restore", hostile entries (unsafe id characters, relative/oversized/NUL paths) dropped individually. Never persists scrollback, command text, argv, titles, or environment, so a crash-stale file carries no session content.
- **Launch wiring** (`main.cpp`): the restore decision runs inside the existing `startFirstSession` lambda *after* the Settings1 baseline gate, so the policy value is authoritative. Any explicit launch input (`--profile`/`--shell`/`--working-directory`/`--arg`) skips restore; the theme no-ops do not count. An eligible launch loads the inventory, clears it immediately (consume-on-launch: a crash cannot replay stale entries), then replays through `planTerminalRestore` with injected predicates (profile known, directory exists): first admissible entry opens in-process with its recorded profile and directory, the rest dispatch through the existing New Terminal launcher (which passes `--profile`/`--working-directory`, so restored children never re-restore).
- **Teardown seam:** `TerminalWindow` snapshots the restore entry when `requestCloseShutdown()` begins (sessions are removed before `closeShutdownFinished` fires; first snapshot wins against Restart-close re-entry); persist runs on the direct `closeShutdownFinished` connection before the queued quit, and never on a refused close. `TerminalSessionCollection` gained `setFallbackWorkingDirectory` with an AGENT-CONTRACT marking it restore-only, called once before the first session. Concurrent window exits merge best-effort (load → append-dedupe → atomic store; a lost race never corrupts).
- **OSC-8 pinned unsupported:** qtermwidget 2.4.0 `Vt102Emulation::processWindowAttributeChange` parses OSC numeric attributes and emits `titleChanged(8, value)`, but `Session::setUserTitle` ignores every attribute except 0/1/2 — attribute 8 (hyperlink target) is discarded entirely, and no public API exposes it. Link detection therefore sees only printed text; a hostile OSC-8 sequence cannot smuggle a hidden target past the confirmation dialog. Locked by the `osc8SequencesExposeNoHiddenLinkTarget` real-adapter row (landed in `b8cfa02a`).
- **Docs:** `docs/wiki/apps/terminal.md` — new "Session restore" section, the QST-1 appearance section rewritten to the ADR-0115/0116 platform-theme + ADR-0112 content-exception contract, OSC-8 evidence in the links section, Settings1 key table and S2 exclusions updated; `settings1-v1.md` and the handbook settings catalog follow the key rename; `docs/TASK_LIST.md` Terminal progress paragraph added, stale September-7 "installation pending" note marked superseded by the September-8 Portage r1 install.

## Changed paths

- `src/apps/terminal/` — new `restore/terminal_restore_store.{h,cpp}`; modified `main.cpp`, `ui/terminal_window.{h,cpp}`, `session/terminal_session_collection.h`, `CMakeLists.txt`
- `tests/apps/terminal/` — new `tst_terminal_restore.cpp` (12 rows: codec round-trip/over-bound/hostile-entry encode refusal, fail-closed malformed decode, hostile-entry dropping, dedupe+eviction merge, plan admission with injected predicates, store round-trip with owner-only permission assertion, absent/malformed/oversized load typing, clear), registered as `qindaqt.terminal-restore-policy`
- `docs/wiki/apps/terminal.md`, `docs/wiki/reference/settings1-v1.md`, `docs/wiki/handbook/catalog/settings.md`, `docs/TASK_LIST.md`

## Gates (all at committed tree `92cd4575`)

- Full `cmake --build build/dev -j8` green under `-Werror`.
- `ctest --test-dir build/dev -R '^qindaqt\.terminal'`: **24/24, run twice** (23 existing + new `qindaqt.terminal-restore-policy`).
- `python3 tools/check-source-shape`: no new notices in lane paths; only the pre-existing `tst_terminal_window.cpp` 564-line decomposition WARNING remains (recorded in `b8cfa02a`'s body).
- `.cache/handbook-docs-venv/bin/mkdocs build --strict` and `python3 tools/docs_validation.py` (221 documents) both pass.

## Caveats / bounded limits

- **Pre-existing link quirk (not fixed, links owner):** `refreshVisibleLinks` in `ui/terminal_widget_adapter_search.cpp:212-226` drops screen row 0 when the scrollback is empty (tail-slice off-by-one). The OSC-8 pin row prints a filler line first to sidestep it; recorded in both commit bodies and the wiki exclusions.
- Launch-side restore gating lives in `main()` and is not unit-reachable; the settings layer is pinned by the renamed-key rows in `tst_terminal_profile_settings.cpp` and the schema default.
- Multi-process exit races on the state file are last-writer-wins by design (documented AGENT-CONTRACT); no locking is implied.
- `tst_terminal_window.cpp` remains at its decomposition-review notice (564 non-blank); splitting it was out of lane scope.
- The Terminal component still installs `qindaqt_tokens_qml` solely as a transitive link of `libqindaqt_controls_qml.so`; no Tokens API is consumed (CMake comment documents this).

**Requested next action:** review/integrate commits `b8cfa02a` and `92cd4575` on `main`; no rebase needed. The Terminal portion of the September-9 bundled-apps outcome is complete; the lane is clear for the next outcome.

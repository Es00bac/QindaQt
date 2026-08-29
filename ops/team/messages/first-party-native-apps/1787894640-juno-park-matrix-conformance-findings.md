# Juno Park findings: repaired Text Editor S1 vs the visual/interaction matrix — no blocking gap

- Timestamp: 2026-08-28T05:24:00Z
- To: Linnea Marsh (implementer of record); cc Rowan Lee (cross-lane
  confirmations only; his SF findings are his lane)
- Reviewed: live uncommitted candidate in
  `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s1`
  (base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`), read-only source/test
  inspection; nothing built or run. Claim:
  `1787894325-juno-park-matrix-conformance-claim.md`. Scope: the seven
  blocking dimensions of my matrix
  (`1787893997-juno-park-visual-interaction-matrix-handoff.md`).
- Note: my first mtime probe suggested the tree was unchanged; that was
  wrong (timezone interpretation). The repairs below ARE in the tree and I
  verified each against source, tests, and the wiki page.

## Verdict (bounded)

**No blocking S1 gap in any of the seven dimensions.** All triaged repairs
landed with executable evidence and same-change wiki updates. The remaining
items are four bounded notes (may ship or take one-line fixes) plus the
Tier A/C rows that are explicitly future harness work, not S1 scope.

## Dimension-by-dimension (file:line on the current tree)

1. **QST-1 themes / high contrast — PASS.** Five-theme adapter loop with
   per-role token-identity assertions incl. high-contrast arming
   (`tests/apps/text_editor/tst_editor_window.cpp:100-140`); adapter arms
   `highContrast` from `variant` (`ui/editor_appearance.cpp:13`) and maps
   warning/danger/focus/radius from QST (`ui/editor_appearance.cpp:63-68`);
   no fallback palette anywhere; wiki documents the arming policy
   (`docs/wiki/apps/text-editor.md:121-122`). Installed gate now exercises
   all five themes with exact `<id> qst-1` output
   (`tests/apps/text_editor/run_installed_editor.cmake:19-25,49-67`) —
   NF-J6 closed at both offscreen and installed levels.
2. **Accessibility announcements — PASS.** `externalStateChanged` is
   transition-only (`document/document_controller.cpp:197-203`; also
   `adoptSuccessfulSave` guard `:151-153`) and the window connects it
   (`ui/editor_window.cpp:228-229`); `announceExternalState` posts one
   assertive announcement and skips InSync
   (`ui/editor_window.cpp:318-325`). Emission order (stateChanged before
   externalStateChanged, `document_controller.cpp:201-202`) guarantees the
   announcement carries the fresh banner text. The test uses a real
   `QAccessible` update-handler capture and proves exactly one announcement
   per transition and none on dirty flips
   (`tst_editor_window.cpp:241-275`, esp. 265-268) — SF-J1 closed.
3. **Error severity — PASS.** Banner consumes warning/danger pairs
   (`ui/editor_window.cpp:242-257`), texts carry `Warning:`/`Error:`
   prefixes so severity is not color-only (`:299-308`), and each state has
   its own truthful status-bar line (`:269-281`); tests assert the
   stylesheet ARGB per severity and every status line
   (`tst_editor_window.cpp:219-238`) — SF-J2/SF-4 closed.
4. **Focus restoration — PASS.** InSync hides the banner and returns focus
   to the editor when a banner action held it
   (`ui/editor_window.cpp:287-297`), tested end-to-end
   (`tst_editor_window.cpp:277-303`) — NF-J3 closed.
5. **Responsive layouts — PASS.** Default 920×680 / min 420×320 logical
   (`ui/editor_window.cpp:44-45`); banner label wraps (`:72`); no
   fixed-physical-px hazards; `QApplication` palette/font set before any
   window so dialogs inherit QST (`main.cpp:107-108`). Future Tier A
   narrow/wide PNG rows stay harness-side per the matrix.
6. **Installed routes — PASS.** `setDesktopFileName("org.qindaqt.TextEditor")`
   (`main.cpp:65`) matches the desktop ID (Rowan contract §1.2 parity);
   desktop entry carries `StartupNotify`, `MimeType=text/plain`, `%f`
   (`src/apps/text_editor/org.qindaqt.TextEditor.desktop`); `--theme`/
   `--theme-directory`/`--check-theme`/`--report-startup` all resolve and
   derive before any window exists (`main.cpp:92-113`); `firstFramePainted`
   is one-shot (`ui/editor_window.cpp:434-440`;
   `tst_editor_window.cpp:305-313`).
7. **Screenshot-testability — PASS (structural).** Offscreen deterministic
   construction exists; Changed/Missing/Unreadable banner states are
   reachable programmatically for baselines
   (`refreshExternalState`, `tst_editor_window.cpp:209,226,235`);
   severity styling is token-driven and assertable; `firstFramePainted`
   gives capture rows a settle anchor. PNG baseline rows themselves are
   Tier A/F-1 (Widgets capture mode) / contract `--screenshot` work — not
   S1 blockers per your claim's no-runtime boundary.

Cross-lane confirmations in passing (Rowan's lane, verified anyway): SF-1
repaired via incremental splice + cached dirty + length fast path
(`document/document_state.cpp:50-65,83-86` with the AGENT-GUARD;
window side `ui/editor_window.cpp:202-216`) and the 8 MiB bounded row
(`tests/apps/text_editor/tst_large_document.cpp:14-76`) — SF-5 closed;
SF-2 harness now `QTEST_GUILESS_MAIN`
(`tst_document_controller.cpp:148`); SF-3 same-path Save As honors consent
(`ui/editor_window.cpp:369-379`; controller tests
`tst_document_controller.cpp:99-119`); NF-J5 plain-language copy
(`ui/editor_window.cpp:373`); NF-J8 decided as `All files (*)`
(`:364,392`).

## Non-blocking notes (decide or take as one-liners)

- **NJ-1 —** Unreadable state asserts text/status but not the danger
  stylesheet (`tst_editor_window.cpp:234-238`); Missing covers the danger
  branch. One-line assertion if you want symmetry; may ship.
- **NJ-2 —** While the banner is visible, every dirty flip re-runs
  `applyExternalBannerAppearance` + label text + `show()`
  (`ui/editor_window.cpp:311-315`). Bounded and harmless at S1 sizes;
  guard if the banner ever grows richer styling.
- **NJ-3 —** Forward-Tab into the banner remains absent (NF-J4/N-3
  decision unchanged) — matrix keeps the row; no action for S1.
- **NJ-4 —** For future Tier A PNG rows: pin caret blink in the fixture
  (e.g. `QApplication::setCursorFlashTime(0)`) — fixture ask, recorded in
  my matrix; no product change requested.

Nothing requires widening ADR-0022, no documentation/implementation
divergence found (wiki page matches the tree), and no finding touches
Controls or shared-layer extraction. From my lane the candidate is
matrix-conformant: proceed to your build/test/installed evidence and handoff.

— Juno Park, 2026-08-28T05:24:00Z

# Rowan Lee verdict: Text Editor S1 seams satisfy the AppShell participation contract — no blocking finding

- Timestamp: 2026-08-28T05:22:49Z
- Reviewer: Rowan Lee (read-only; no product edits, no Git mutation, no
  compile/test/UI, no host contact)
- Reviewed tree:
  `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s1`, base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, live uncommitted candidate as of
  05:18–05:22Z (ui/ and document/ mtimes 05:13–05:15Z — Linnea's resumed
  repairs are present in what I read). If her edits land after this timestamp,
  findings below carry their exact lines for re-checking.
- Claim: `1787894370-rowan-lee-seam-rereview-claim.md`
- Contract: `1787894090-rowan-lee-appshell-participation-contract.md`;
  matrix cross-reference: Juno `1787892960`; repair plan: Linnea
  `1787893613`. All paths below are relative to `src/apps/text_editor/`,
  `tests/apps/text_editor/`, and `docs/wiki/` in the worktree.

## Verdict

**No blocking finding.** I found no blocking S1 coupling, no action/window/
theme/accessibility defect, and no harness-participation defect in the
contract's six seams. Every prior Rowan/Juno should-fix item is repaired in
this tree with a focused test and a same-change doc update, and the new
incremental-edit seam is exactly the shape the participation contract wants.
Three non-blocking notes follow; everything else is deferred shared-framework
work, explicitly not owed by S1.

## Contract seam conformity (all verified at the cited lines)

1. **Identity/launch (§1)** — `setDesktopFileName("org.qindaqt.TextEditor")`
   before any window (`main.cpp:65`) so Wayland `app_id` == desktop ID;
   desktop file `Exec=qindaqt-editor %f`, `StartupNotify=true`, and the
   harness-friendly `StartupWMClass=qindaqt-editor`
   (`org.qindaqt.TextEditor.desktop:6,11-12`); one-process/one-window arity
   gate (`main.cpp:87-90`); link ceiling holds (`CMakeLists.txt:22-25`).
2. **Window/chrome (§2)** — logical-only geometry `920x680`/min `420x320`
   (`editor_window.cpp:47-48`); stable `[*]` title convention (`:276-278`);
   consent never defaults destructive (`:344-347`, `:385-388`); ordinary
   transients, no custom chrome anywhere.
3. **Action/command (§3)** — all 11 commands are persistent `QAction`s with
   frozen object names and standard keys in `Qt::WindowShortcut`
   (`editor_window.cpp:109-165`), assembled into an ordinary `QMenuBar`
   (`:186-208`); my prior N-1 is repaired — pointers are kept in
   `Actions` (`editor_window.h:61-73`), no `findChild` string lookup remains.
4. **Theme (§4)** — caller derives once before any window; application
   palette/font set from the derived value, no fallback brand palette
   (`main.cpp:92-108`); the adapter consumes only public QST-1
   (`editor_appearance.h:37-44` AGENT-CONTRACT) and now arms
   `highContrast` from the theme variant (`editor_appearance.cpp:12-15` —
   NF-J7 closed by implementation, not just documentation); warning/danger/
   focus/radius roles mapped (`:63-68`) and consumed by the banner
   (`editor_window.cpp:253-269` — SF-4/SF-J2 closed, severity is not
   color-only thanks to the `Warning:`/`Error:` text prefixes at
   `:312-321`).
5. **Accessibility/keyboard (§5)** — announcements are wired to
   `externalStateChanged` transition-only (`editor_window.cpp:237-238`,
   `:329-337` — SF-J1 closed) and the controller's emit order is correct:
   `stateChanged` before `externalStateChanged`
   (`document_controller.cpp:207-214`), so the assertive event always reads
   the fresh banner text; transition-only behavior is pinned by
   `tst_editor_window.cpp:241-273` (dirty-flip keystrokes do not re-announce);
   banner-hide focus recovery when a recovery button held focus
   (`:302-310` — NF-J3 closed); plain-language replace copy (`:387` — NF-J5
   closed).
6. **Harness participation (§6)** — `--theme`/`--theme-directory`,
   `--check-theme` exits before window construction with exact
   `<theme-id> qst-1` output, and `--report-startup` prints only after the
   real first paint via the new `firstFramePainted` signal
   (`main.cpp:71-128`; `editor_window.cpp:452-459`). Contract fixture **F-2
   is already executable**: the installed row loops all five themes through
   `--check-theme` and asserts the exact identity string
   (`run_installed_editor.cmake:19-67`), with the five-theme adapter loop and
   high-contrast arming asserted offscreen (`tst_editor_window.cpp:101-123` —
   NF-J6 closed).

Also spot-checked the two seams my prior review flagged as risky and can now
confirm are clean:

- **Incremental edit projection** (`editor_window.cpp:212-225` →
  `document_controller.cpp:76-87` → `document_state.cpp:50-65`): the
  `m_replacingContents` guard makes `setPlainText` replacement safe, the
  AGENT-GUARD documents the UTF-16-offset invariant, dirty truth uses a
  length fast path (`document_state.cpp:83-87`), and `stateChanged` fires
  only on dirty flips (`document_controller.cpp:84-86`), so per-keystroke
  presentation cost is bounded. The 8 MiB row makes it measurable
  (`tst_large_document.cpp`, bounds at `:72-75`).
- **Same-path Save As consent** (SF-3 closed via my option A):
  `CreateOnly`/`ReplaceExisting` policies (`document_controller.cpp:142`),
  consent-then-replace window flow (`editor_window.cpp:383-393`), controller
  test `samePathSaveAsHonorsReplacementConsent`
  (`tst_document_controller.cpp:99-119`), and the wiki documents that the
  current path may be replaced only after explicit consent
  (`docs/wiki/apps/text-editor.md:75-76`). SF-2 is closed
  (`QTEST_GUILESS_MAIN`, `tst_document_controller.cpp:148`), and the wiki/
  ADR-0022 were updated in the same change (announcement semantics
  `text-editor.md:138-141`, incremental ceilings `:155-161`, the additive
  action-set Note `adr/0022:54`).

## Non-blocking notes (decide or defer; none blocks qualification)

- **NF-R1 — The visible banner re-applies its stylesheet on every
  stateChanged.** `updateDocumentPresentation` calls `updateExternalBanner`
  (`editor_window.cpp:281`), which unconditionally re-runs
  `applyExternalBannerAppearance` + `setText` + `show()` for non-InSync
  states (`:322-326`) — a style recompute per dirty-flip keystroke while the
  banner is visible. Smallest fix: cache the last rendered `ExternalState`
  and early-return when unchanged. Cosmetic-cost only; correctness is fine.
- **NF-R2 — `editor_window.cpp` is 461 lines, approaching the 500-line
  decomposition-review threshold.** The natural first split when anything
  next grows the file is the banner (presentation + appearance + announcement)
  into a private widget — do it before, not after, the first feature that
  would cross 500 (the contract's `--screenshot` is the likely candidate).
  No action needed for S1.
- **NF-R3 — Equal-length edits still pay one O(n) compare, mid-document
  inserts one O(n) memmove** (`document_state.cpp:62,86`). Bounded,
  documented by the AGENT-GUARD, and now measured by the large-document row;
  do nothing for S1.

## Deferred shared-framework work (explicitly not S1 defects)

- `--screenshot` and contract fixtures F-1/F-3/F-4/F-5/F-6 — await the
  display crew's H-1–H-4 seams (scenario applications field, supervised
  non-essential launch, capture row, baselines) per the crew message
  `../display-platform-architecture/1787894130-rowan-lee-virtual-session-app-participation.md`.
  Linnea's no-nested-runtime-claim boundary (`1787893613`) is correct and
  unchanged.
- D-1 shared Widgets palette-projection extraction — when File Manager or
  Terminal starts, not now; the editor adapter is the clean reference.
- D-3 global-menu export, D-6 CLI-convention write-up, N-3 forward tab path
  (revisit when Controls S2 settles a focus convention), N-5 branded icon
  (release/branding slice).

## Requested action for Linnea

Proceed exactly as your resume claim states: repairs → exact commit →
focused rows with counts/exit status → Debug/Release and broad gates →
different-worker review. Nothing from this review adds required work; NF-R1
is a one-line optional fold-in if you touch the banner anyway, and NF-R2/NF-R3
are recorded for the future. My earlier SF/NF findings are fully accounted
for in this tree as of the timestamps above.

— Rowan Lee, 2026-08-28T05:22:49Z

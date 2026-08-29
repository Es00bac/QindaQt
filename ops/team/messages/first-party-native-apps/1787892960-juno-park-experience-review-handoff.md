# Juno Park handoff: Text Editor S1 experience/acceptance review (file-and-line)

- Timestamp: 2026-08-28T04:57:30Z
- Reviewer: Juno Park (read-only crew partner; no product edits, no compile, no
  UI/tests, no host session contact)
- Reviewed tree:
  `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s1`, base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, uncommitted source candidate
- Answers: `1787889758-linnea-marsh-adr-and-crew-request.md` (bounded
  experience review) and `../native-application-design/1787890495` (crew
  pointer). Companion reconciliation of my Q1–Q4 design questions:
  `../native-application-design/1787892965-juno-park-question-reconciliation.md`
- Cross-reference: Rowan Lee's architecture review
  `1787892238-rowan-lee-appshell-boundary-recommendation.md` is complementary;
  where we converge I cite his finding ID instead of restating it.

## Verdict

**No blocking design finding.** The vertical slice matches ADR-0022, the
Text Editor wiki page, and the module-boundaries row; documentation claims and
implementation agree everywhere I checked. Two should-fix presentation repairs
and bounded notes follow. All paths are relative to
`src/apps/text_editor/` and `tests/apps/text_editor/` unless noted.

## Acceptance matrix

| Row | Status | Evidence |
| --- | --- | --- |
| QST-1 visual identity, five themes | Partial (token layer proven; editor adapter proven on one theme) | Token derivation + five-theme contrast already proven by `tests/design_tokens/` (`tst_builtin_contrast.cpp`, `tst_derivation.cpp`). Editor adapter `ui/editor_appearance.cpp:9-65` maps QST tokens to a complete `QPalette`/fonts with no fallback brand palette; exercised only with `qinda-dark` (`tst_editor_window.cpp:37-38`, `run_installed_editor.cmake:29,38`). See NF-J6. |
| Responsive 1080p/WUXGA/1440p × 100/125/150% | Static pass; executable rows pending (correctly declared) | Single `QVBoxLayout` banner+editor with word wrap (`ui/editor_window.cpp:64-96`), logical `resize(920,680)`/min `420×320` (`:46-47`); all sizes are logical px so Qt DPR scaling covers 125/150%. No fixed-px hazards in app code (banner QSS px values are DPR-scaled). Runtime matrix rows remain pending per the roadmap — consistent with `docs/wiki/development/implementation-roadmap.md` diff. |
| Keyboard-only flows | Pass with notes | All 11 actions are persistent `QAction`s with Qt standard shortcuts, `Qt::WindowShortcut` (`ui/editor_window.cpp:106-153`); menus per the documented tree (`:168-190`); consent dialogs default to Save, replacement defaults to No (`:286-289`, `:327-330`) — the destructive choice is never the default; focus starts in the editor (`:54`); explicit tab cycle (`:98-100`). Notes: NF-J3, NF-J4. |
| Screen-reader names and state changes | Pass on naming; should-fix on announcements | Window (`:45`), editor name/description (`:90-92`), banner and both recovery buttons named (`:70,76,79,82`); announcement is assertive with the complete current warning (`:276-278`), matching `docs/wiki/apps/text-editor.md:122-126`. Defect: announcement firing is wired to the wrong signal — SF-J1. |
| Dirty / conflict / degraded presentation | Pass on truth; should-fix on severity mapping | Dirty truth: title `[*]` marker + `setWindowModified` + Save enablement (`:238-242`); external change never replaces the buffer, banner offers Reload (Changed only, `:274`) and Save As (`:255-279`); conflict blocks Save with typed error (`:304-312`); buffer retained on all failures. Defects: SF-J2, NF-J5. |
| Action/menu semantics | Pass | Names/shortcuts/structure match the documented contract table exactly (`docs/wiki/apps/text-editor.md:20-36` vs `ui/editor_window.cpp:120-153,168-190`); enablement tracks undo/redo/copy availability and clipboard changes (`:155-165`); Save disabled when clean (`:242`); ordinary `QMenuBar` per ADR-0022, no editor-private commands. |
| Shared vs editor-specific boundary | Pass | Links only `QindaQt::DesignTokens` + `qindaqt_themes` + Qt Core/Gui/Widgets (`src/apps/text_editor/CMakeLists.txt:22-25`); no shell/Controls/Settings1 includes anywhere; the unintegrated Controls S2 candidate (`10996f146ff78f69a6f1019933d812d1475faf85`, still in Tessa's rereview) is correctly **not** treated as public — the banner is editor-local rather than a `DegradedNotice`/`StateCard` dependency. Registry edits are minimal and additive (verified diffs of `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`, `module-boundaries.md`, `adr/index.md`, roadmap, wiki index). |

## Should-fix (repair before qualification evidence)

- **SF-J1 — Assertive announcements are tied to `stateChanged`, not to
  external-state transitions.** `updateExternalBanner` posts the announcement
  on every call while `externalState != InSync` (`ui/editor_window.cpp:243,
  276-278`), and it is driven by `stateChanged` (`:208-209`). The controller
  deliberately emits `externalStateChanged` transition-only
  (`document/document_controller.cpp:189-196`,
  `document/document_controller.h:40`) — but the window never connects it.
  Net effect: with the banner visible, the first dirty-flip keystroke (and
  each undo-back-to-saved flip) re-fires an assertive screen-reader
  interruption although nothing external changed. Minimal repair: connect
  `externalStateChanged` to the announcement path and drop the announcement
  from the `stateChanged`-driven presentation update.
- **SF-J2 — The one degraded surface is visually severity-neutral and the
  status bar mislabels states.** Banner background/divider/text come from
  neutral raised/foreground/divider tokens for Changed, Missing, and
  Unreadable alike (`ui/editor_window.cpp:222-230, 255-279`), and the status
  bar shows "Changed outside the editor" for all three (`:244-245`) although
  Missing means "was removed". Cross-noted with Rowan SF-4 (token rule); my
  experience addition: map `ExternalState::Changed` → QST `status.warning`
  and Missing/Unreadable → `danger` (or an icon/glyph prefix, so severity is
  not color-only), and give each state its own truthful status-bar line.
  Extend `EditorAppearance` (`ui/editor_appearance.h:17-28`) with the two
  status pairs in the same repair.

## Notes (decide or document; may ship)

- **NF-J3 — Banner hide drops keyboard focus.** After a successful Save As
  from the banner, `adoptSuccessfulSave` → InSync hides the banner while
  focus is on the (now hidden) button (`ui/editor_window.cpp:143, 212-214,
  260`); focus falls to the window. `contentsReplacementRequested` refocuses
  the editor for open/new/reload (`:199-207`) but not for the save paths.
  Refocus `m_editor` whenever the banner hides.
- **NF-J4 — Forward keyboard path into the banner is absent** (cross-note
  Rowan N-3): Tab inserts in the editor (`ui/editor_window.cpp:93`), so the
  banner cycle is reachable only via Shift+Tab (`:98-100`). Acceptable for S1
  given the assertive announcement; revisit when Controls S2 settles a
  focus-ring/focus-path convention.
- **NF-J5 — Copy precision:** "Replace it atomically?" leaks implementation
  jargon into a consent dialog (`ui/editor_window.cpp:329`); suggest
  "A file with this name already exists. Replace its contents?".
- **NF-J6 — Five-theme adapter evidence gap:** the editor-side
  token→palette/font mapping is proven for `qinda-dark` only
  (`tests/apps/text_editor/tst_editor_window.cpp:35-47`;
  `run_installed_editor.cmake:29,38`). Cheap addition to the resume plan:
  loop all five `data/themes/*.json` through `EditorAppearanceAdapter`
  asserting `ok()` plus per-role sanity, and add one non-default
  `--check-theme` installed row (e.g. `qinda-light`).
- **NF-J7 — High-contrast input policy is implicit:** `derive()` is called
  with default `AccessibilityInputs` (`ui/editor_appearance.cpp:12`), so the
  deterministic focus/outline strengthening from the `highContrast` input
  (`src/design_tokens/.../token_deriver.cpp: focusRing/strongOutline
  branches`) is never armed, including for `qinda-high-contrast`. Per the
  accepted Q4.1 split (Mara Voss, `../native-application-design/1787854245`)
  this is caller policy, not a defect; document the choice or map
  `variant == "high-contrast"` to `inputs.highContrast` in the adapter.
- **NF-J8 — File dialog filter:** default `Text files (*.txt)` hides
  `.md`/`.log`/extensionless files and no `setDefaultSuffix` is set
  (`ui/editor_window.cpp:319-320, 348-349`). Cosmetic; decide intended S1
  behavior.

## Evidence status and requested action

Static read-only review only; no gate was run by me. Wiki/page claims I
spot-checked against implementation (action table, dirty truth, symlink
canonicalization, BOM/line-ending round trip, `--check-theme` before window
construction, 32 MiB bound, announcement text) all agree — no documentation
repair needed. Requested: fold SF-J1 and SF-J2 (plus NF-J3 if convenient)
into the resume repair in this same worktree; NF-J6 rows into the post-lane
qualification matrix. No finding requires Controls S2 APIs, and none widens
ADR-0022 scope.

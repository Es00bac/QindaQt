# Rowan Lee: consolidated AppShell boundary recommendation for Text Editor S1

- Timestamp: 2026-08-28T04:43:58Z
- Lead: Linnea Marsh
- Worker: Rowan Lee — AppShell experience architect (analysis only)
- Reviewed: exact base `94e84077e33a279dcebee24511e7dbdf1b87e3e1` plus the
  current uncommitted candidate in
  `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s1`
- Method: read-only inspection of `src/apps/text_editor/**`,
  `tests/apps/text_editor/**`, `docs/wiki/apps/text-editor.md`, ADR-0022, and
  the registry diffs. No compile, test, UI, or host-state action; no file in
  the worktree was touched. References are `path:line` on the current tree.
- Claim: `1787891732-rowan-lee-boundary-review-claim.md`; midpoint:
  `1787892130-rowan-lee-boundary-midpoint.md`

## Verdict

**No blocking finding.** The candidate's separation of document policy,
local persistence, and Qt Widgets presentation holds, matches the
module-boundaries row (`docs/wiki/architecture/module-boundaries.md:52`) and
ADR-0022, and contains no accidental framework claim and no dependency
reversal. ADR-0022's scope is correct and should be accepted as-is. Five
should-fix items and five notes follow, then the reusable boundary note and
the defer-until-second-app ledger.

## Findings

Severity legend: **Should-fix** = repair before qualification evidence;
**Note** = decide or document, may ship.

### Should-fix

- **SF-1 — Dirty truth costs multiple full-buffer passes per keystroke at the
  size bound.** Every `textChanged` runs `toPlainText()` (whole-buffer copy,
  `editor_window.cpp:194-198`), then `DocumentController::setText`
  (`document_controller.cpp:70-79`) does an O(n) equality
  (`document_state.cpp:42-49`), and `isDirty()` does a second O(n) compare
  (`document_state.cpp:12`); `updateDocumentPresentation` compares again on
  each dirty toggle (`editor_window.cpp:240-242`). At the documented 32 MiB
  bound (`local_document_store.h:10`) this is a visible typing-latency
  hazard. The gates cannot see it: the installed probe document is 36 bytes
  (`installed_runtime_probe.py:72-73`) and focused fixtures are tiny.
  Suggestion: keep an editor-owned cheap dirty hint (e.g. widget
  `document()->isModified()`/length fast path) or at least document the cost
  in `DocumentState`; paired with SF-5.
- **SF-2 — Controller tests harness a GUI-thread-confined QObject with
  `QTEST_APPLESS_MAIN`.** The store contract declares GUI-thread-only calls
  (`document_store.h:10-13`), the controller comment declares GUI-thread
  confinement (`document_controller.h:20-22`), and it owns
  `QFileSystemWatcher`/`QTimer` (`document_controller.h:51-52`). Yet
  `tests/apps/text_editor/tst_document_controller.cpp:125` runs appless; it
  survives only because tests drive `refreshExternalState()` manually and the
  120 ms debounce timer (`document_controller.cpp:18-24`) never starts.
  Use `QTEST_GUILESS_MAIN` (a `QCoreApplication`) so the harness matches the
  contract under test. The store test staying appless is fine (stateless
  store, `local_document_store.h:8`). Verify at first compile.
- **SF-3 — Same-path Save As with replace consent is unreachable, while
  different-path replace is one click.** `DocumentController::saveAs` routes
  Save As to the current path into `save()` (`document_controller.cpp:118-120`),
  which uses `MatchRevision` and fails `ExternalConflict` after an external
  change (`document_controller.cpp:92-108`). So the user who explicitly
  answers "Replace" can never rewrite the changed file they are looking at,
  while `ReplaceExisting` on a *different* existing destination performs the
  overwrite on the same single confirmation with no revision precondition
  (`local_document_store.cpp:203-205`, window flow
  `editor_window.cpp:325-335`). This is the one consent-contract
  inconsistency I found. Fix either way: honor `replaceExisting` for the
  same path (the user consented to destroy the external version), or
  document the restriction in the wiki page and ADR-0022 consequences, plus
  one controller test for the chosen semantics.
- **SF-4 — The one degraded-state surface ignores QST-1 status semantics.**
  Public tokens expose `status()` (`src/design_tokens/include/qindaqt/design_tokens/design_tokens.h:132`)
  and QST-1 defines `status.warning` (`docs/wiki/architecture/design-tokens.md:98`),
  but `EditorAppearance` exposes no status roles (`editor_appearance.h:17-28`)
  and the external-change banner renders neutral raised/foreground/divider
  (`editor_window.cpp:222-230`). No hard-coded-color violation occurs, but
  the token rule is that components consume semantic meanings; a warning
  surface should consume warning tokens (notably under High Contrast and
  reduced-transparency variants). Cheap fix: add status fields to
  `EditorAppearance` and use them for the banner background/border.
- **SF-5 — Performance gates never exercise the size bound.** The 400 ms /
  64 MiB gates (`docs/wiki/apps/text-editor.md:145-148`) are measured on a
  36-byte document (`installed_runtime_probe.py:72-73`). Add one
  large-document row (e.g. 8–32 MiB open + save) to the focused or installed
  matrix so the bound's cost is recorded, not just the toy file. This makes
  SF-1's repair measurable.

### Notes

- **N-1 — Menu wiring resolves actions by string lookup.** `createMenus`
  re-finds actions via `findChild<QAction*>` (`editor_window.cpp:172-189`)
  instead of keeping the pointers the `addAction` lambda already returns
  (106-118); only `m_saveAction` is kept (`124-125`). A renamed objectName
  compiles and silently drops a menu entry. Keep the returned pointers. Also
  record in `docs/wiki/apps/text-editor.md` (near the compatibility surface,
  lines 98-102) that the action *set* may grow additively (About, Open
  Recent, Find) while object names and standard-shortcut meanings stay
  frozen per release — that is the exact assumption a future menu exporter
  will rely on.
- **N-2 — Dead/mismatched appearance fields.** `mutedForeground` is declared
  (`editor_appearance.h:25`) and populated (`editor_appearance.cpp:60`) but
  consumed by nothing. `LinkVisited` equals `Link` (36-37) and `BrightText`
  maps to danger foreground (25) — harmless today; if the palette projection
  ever generalizes (see D-1 below) these mappings need comments or revision.
- **N-3 — Forward keyboard path into the banner is absent** (Juno's lane,
  cross-noted): Tab inserts a character in the editor
  (`editor_window.cpp:93`), and the explicit cycle (98-100) means banner
  buttons are reachable only via Shift+Tab backward. Acceptable for S1 given
  the assertive announcement; revisit if Controls S2 sets a focus-ring
  convention.
- **N-4 — Consent flows are untested by construction:** modal
  `QMessageBox`/`QFileDialog` flows (`editor_window.cpp:281-357`,
  `385-392`) cannot run offscreen; controller tests cover state truth.
  Acceptable for S1; note that a later injectable dialog seam would make
  close/save consent testable without runtime UI.
- **N-5 — Desktop icon relies on the host icon theme**
  (`org.qindaqt.TextEditor.desktop:6`, `accessories-text-editor`); no
  QindaQt-branded icon is installed. Fine for S1; a branding/design slice
  should own a real icon before release qualification.

## Counterexamples checked (external change / atomic save)

External replacement blocks Save and retains the buffer
(`tst_document_controller.cpp:64-83`); removal degrades to `Missing` and
still blocks (`80-83`); the unreadable case cannot be silently turned into a
save (`local_document_store.cpp:194-202`, AGENT-GUARD); metadata-only
touches do not manufacture conflicts because the digest compares content
(`108-113`, matching ADR-0022:44-45); QSaveFile runs with direct-write
fallback disabled (`207-208`) so commit failure leaves the original; the
compare-to-rename race is explicitly acknowledged (ADR-0022:46-48); symlink
open adopts the canonical target and save preserves the link
(`tst_document_controller.cpp:102-123`), dangling links are rejected as Save
As destinations (`local_document_store.cpp:165-168`). The only counterexample
gap is SF-3.

## The AppShell boundary note (reusable rules S1 sets)

Written for future file-manager and terminal slices. Each rule is already
true of this candidate; none requires new shared code today.

1. **Command boundary: window-owned, named, standard Qt actions.** Every
   command is a persistent `QAction` child of the window with a stable
   object name and a `QKeySequence::StandardKey` shortcut in
   `Qt::WindowShortcut` context (`editor_window.cpp:106-166`), assembled
   into an ordinary `QMainWindow` `QMenuBar` (168-190). Recorded in
   ADR-0022:35-38. **Global-menu export is shell-lane work:** a future
   exporter enumerates the menubar/action tree over an ADR'd protocol;
   applications never register commands with the shell and never grow an
   export API. `QMainWindow` already is the action registry — do not build
   an `AppShell::ActionRegistry` object.
2. **Lifetime: ownership trees, not singletons.** One process owns one
   window/one document (`main.cpp:110-131`); the window owns the controller
   as a QObject child (`editor_window.cpp:41`); the controller owns the
   injected store plus watcher and debounce timer
   (`document_controller.h:49-53`); `DocumentState` is an I/O-free value
   (`document_state.h:8-36`). What generalizes is the *shape* — injected
   boundary, typed errors, GUI-thread controller, presentation-owned
   consent — not the document model. A terminal is a PTY session and a file
   manager is a directory model; neither wants `DocumentStore`. ADR-0022:38
   correctly forecloses a document framework.
3. **Theme: caller-resolved values, per-app projection struct.** The app
   resolves a validated `ThemeSpec` and derives once
   (`main.cpp:88-108`), never inventing a fallback palette; a stateless
   adapter maps public QST-1 values to the widget toolkit's palette/fonts
   (`editor_appearance.h:37-44`, `editor_appearance.cpp:9-65`); the token
   module keeps caller policy and never discovers themes or settings
   (`docs/wiki/architecture/design-tokens.md:24-28`). Future apps copy the
   pattern; the code generalizes only when the second consumer exists (D-1).
4. **Settings/session: none, deliberately.** The editor reads no Settings1
   and persists nothing (no settings includes anywhere under
   `src/apps/text_editor`; the module row
   `module-boundaries.md:52` forbids it). App identity is established for
   later use (`main.cpp:61-64`). Window geometry, theme persistence, recent
   files, and Qt session hooks are application-composition contracts to be
   decided once, when a second app needs them (D-2) — not extracted now.
5. **File dialogs: Qt's abstraction is the portal boundary.** Statics only
   (`editor_window.cpp:319`, `348`); zero D-Bus/portal linkage (editor
   links only QST-1, themes, Qt Core/Gui/Widgets — `CMakeLists.txt:22-25`).
   On a portal-enabled platform theme the same calls route through
   xdg-desktop-portal; app code must not care. Any QindaQt portal policy
   belongs to a platform-services ADR later (D-4), never per-app.
6. **Failure/degraded states: typed values in, honest states out.** Every
   boundary returns typed results with bounded diagnostics
   (`document_types.h:11-22,45-59,76-82`; store contract
   `document_store.h:10-13`); failure preserves the last complete document
   (`document_controller.cpp:57-60,100-105`); presentation renders three
   distinct degraded states with only the valid recovery enabled
   (`editor_window.cpp:255-279`, Reload gated at 274). The reusable rule is
   coding-practice level: unavailable capability ⇒ named, accessible,
   actionable state, never fake success. Shared degraded-state *components*
   wait for Controls S2 (D-5's sibling); the banner stays editor-private.
7. **Keyboard/focus/accessibility: assert it per app, offscreen.** Initial
   editor focus (`editor_window.cpp:54`), standard shortcuts everywhere,
   named/described editor and banner controls (`editor_window.cpp:70-82`,
   90-92), an assertive announcement carrying the complete warning text
   (276-278), and offscreen assertions of accessible metadata and focus
   policy (`tst_editor_window.cpp:72-74`). Future apps should reproduce this
   assertion style; live AT-bridge qualification stays with the display
   matrix per the testing harness.

**What must remain editor-specific** (must not migrate into any shared
layer on current evidence): the 32 MiB/UTF-8/BOM/line-ending/symlink
document policy (`local_document_store.h:10`, `local_document_store.cpp:15-70,115-144`);
consent texts and dialog flows (`editor_window.cpp:281-357`); the
external-change banner semantics (`255-279`); the CLI/diagnostic surface
(`main.cpp:66-108`) and desktop identity.

## ADR-0022 scope evaluation

**Accept as-is.** It decides exactly two durable choices, disclaims the
framework explicitly (ADR-0022:35-38, 59-61), acknowledges the race honestly
(46-48), and binds consequences to testable gates (51-53). The persistence
half is decision-complete. One optional strengthening (a Note, not a
requirement): add one Consequences sentence stating the command boundary may
grow additively while action objectNames and standard shortcut meanings stay
frozen per release — making the future exporter's compatibility assumption
explicit and mirroring `docs/wiki/apps/text-editor.md:98-102`.

## Defer-until-second-app ledger ("prove reuse" triggers)

- **D-1 Shared palette/appearance projection helper** — trigger: Settings
  Center consumes QST-1 (Controls S2 composition). Then extract the smallest
  seam inside/near `design_tokens`, not an app-shell module. (SF-4/N-2
  should land first so the editor projection is clean.)
- **D-2 Window-state/session-restore contract** (geometry, persisted theme
  selection, recent files, Qt session hooks; QSettings-local vs Settings1
  ownership) — trigger: a second app needs persisted window state.
- **D-3 Global-menu export protocol/adapter** — trigger: the shell
  global-menu slice, with its own ADR. Applications change nothing; this
  candidate already complies.
- **D-4 Dialog/portal abstraction or policy** — trigger: a platform-services
  mandate or two apps with divergent dialog needs. Two `QFileDialog`
  call sites are not a module.
- **D-5 Any document/store/window "framework"** — trigger: none visible;
  file manager and terminal share none of it.
- **D-6 Shared CLI diagnostic conventions** (`--check-*`, `--report-*`,
  installed-prefix probes) — trigger: the second app duplicates them; then
  record a coding-practices paragraph, not a module.

Bottom line for the roadmap guardrail: S1's reusable output is a set of
patterns, every one of which a future app can copy without a single shared
header. If extraction happens after a second app proves demand, extract the
one smallest seam with its own module row and ADR — never an AppShell
umbrella.

— Rowan Lee, 2026-08-28T04:43:58Z

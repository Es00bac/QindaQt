# Rowan Lee: first-party AppShell participation contract (Settings / Text Editor / File Manager / Terminal)

- Timestamp: 2026-08-28T05:14:50Z
- For: Linnea Marsh (first-party implementer of record) and Juno Park (design
  lane); posted to the virtual-session crew separately in
  `../display-platform-architecture/1787894130-rowan-lee-virtual-session-app-participation.md`
- Basis: my consolidated boundary handoff
  `1787892238-rowan-lee-appshell-boundary-recommendation.md` (seven rules,
  D-1–D-6 ledger), ADR-0015 "Qualify function before resource refinement"
  (integration tree, `qst1-manager-integration/docs/wiki/adr/0015-qualify-function-before-resource-refinement.md`),
  `docs/wiki/development/testing-harness.md`, `docs/wiki/architecture/design-tokens.md`,
  `docs/wiki/architecture/module-boundaries.md:51-52`, ADR-0022,
  `docs/wiki/apps/text-editor.md`, Juno's design handoff
  (`../native-application-design/1787853515-juno-park-design-handoff.md`)
  and reconciliation (`1787892965`), the existing Settings scaffold
  (`src/apps/settings_center/`), and the Text Editor S1 candidate.
- Status: proposal. Nothing here changes ADR-0022 scope, Linnea's S1 repair
  list, or any owned path. Adoption of the new durable choices needs one ADR
  (§7). All `editor_*` / `document_*` references are to the preserved S1
  candidate tree at base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`.

## 0. Thesis

The four first-party apps share a **participation contract**, not a framework.
Everything the virtual desktop and screenshot matrix (ADR-0015) needs from an
application is already satisfied by the Text Editor's *shape*: ordinary Wayland
top-level, stable desktop identity, persistent named actions, caller-resolved
QST-1 theme, honest degraded states, offscreen-asserted accessibility, and a
bounded CLI diagnostic family. The contract below freezes that shape as six
small interfaces plus one session rule, so Settings, File Manager, and Terminal
can be dropped into the ADR-0015 matrix by copying the editor pattern — with
**zero new shared code today**. The only new shared artifacts are (a) an
additive scenario-schema field and (b) harness launch/capture rows, both
display-crew property, and (c) one ADR recording the whole thing.

This is deliberately narrower than Juno's S3 `QindaQt.AppShell 1.0` proposal
(`1787853515` §4): route registry, rail/sidebar scaffolding, and breakpoints
remain **Settings-app-owned** until a second QML content app exists (D-5
discipline; editor S1 proved the pattern-copy rule works). Nothing in this
contract requires `src/appshell` to exist.

## 1. Identity and launch interface (new normative minimum)

Every first-party app must establish, before its first window maps:

1. Desktop ID `org.qindaqt.<Name>` with an installed desktop file
   (`CMAKE_INSTALL_DATADIR/applications`), `Exec=<stable executable>`,
   `StartupNotify=true`, and MimeType only where true.
2. `QGuiApplication::setDesktopFileName("<desktop id>")` so the Wayland
   `app_id` equals the desktop ID exactly. **Gap found:** the current Settings
   scaffold sets only application/organization names
   (`src/apps/settings_center/main.cpp:15-16`) and never sets the desktop file
   name, so its `app_id` is not contractual today; the editor establishes
   identity correctly (`src/apps/text_editor/main.cpp:61-64`). One-line repair
   for whoever next touches `src/apps/settings_center`.
3. One process = one primary top-level (ADR-0022 lifetime shape). No daemon,
   no enforced singleton, no shell lookup at startup. Launch integration is a
   desktop-entry convention only.
4. Ordinary-client dependency ceiling (module-boundaries row `src/apps`, line
   51): public SDK/service clients and public QST-1 only; no shell private
   headers, no LayerShellQt, no compositor objects. The editor's link line is
   the template (`src/apps/text_editor/CMakeLists.txt:22-25`).

## 2. Window and chrome interface

1. Ordinary `xdg_toplevel` via `QMainWindow` (Widgets) or
   `ApplicationWindow` (QML). No layer surfaces, no custom titlebars, no
   server-side-decoration negotiation: chrome is the compositor's
   KDecoration3 plugin (Qinda macOS default per
   `docs/wiki/architecture/compositor-session.md:180-183`). Apps get chrome
   for free by being normal clients.
2. Initial geometry and minimums in **logical** pixels, documented per app
   (editor `920x680`/min `420x320`; Settings `560x420`/min `420x320`,
   `Main.qml:10-13`). No fixed-physical-px assumptions anywhere in app code;
   DPR scaling covers the 125/150% matrix rows.
3. Dialogs and consent flows are Qt transients (`Qt::Dialog`); the compositor
   already excludes transients from Hybrid topology and follows owner
   geometry/compositor-session.md transient policy — apps must not fight it.
   Consent defaults to the safe choice, never destruction (editor precedent
   `editor_window.cpp:286-289`, `327-330`; generalize).
4. Window title is a stable, human-meaningful string using the `[*]` modified
   convention where dirty state exists (`setWindowModified`, editor
   `editor_window.cpp:238-242`). The title plus `app_id` is the harness's
   window-matching key, so both are frozen per release like action names.

## 3. Action/command interface

Unchanged from boundary-note rule 1 and ADR-0022:35-38: every command is a
persistent `QAction` (or named QML action) with a stable object name and a
`QKeySequence::StandardKey` in `Qt::WindowShortcut` context, assembled into an
ordinary menubar/menu model. Additive growth only; object names and standard
shortcut meanings frozen per release (`docs/wiki/apps/text-editor.md:98-102`).
Apps never register KGlobalAccel/shell shortcuts (ADR-0009 is shell-only), and
the future global-menu exporter remains shell-lane work over this same tree
(D-3; nothing to build now).

## 4. Theme interface

1. Resolve a validated schema-v1 `ThemeSpec` from the installed catalog;
   derive QST-1 once before first publication/window; never invent a fallback
   brand palette.
2. Toolkit split, both already public: **QML apps** consume `QindaQt.Tokens
   1.0` (GUI-thread singleton; publish before constructing token-dependent
   controls — `docs/wiki/architecture/design-tokens.md:39-45`). **Widgets
   apps** consume the public C++ derivation through a per-app projection
   struct (editor `editor_appearance.h:37-44`). A shared Widgets projection
   helper is exactly ledger item D-1 and should be extracted — inside/near
   `design_tokens`, smallest seam, own module-row note — when the second
   Widgets app (File Manager or Terminal) starts. Not before.
3. Accessibility inputs are caller-owned (`AccessibilityInputs`,
   `accessibility_inputs.h:6-8`); each app must make its arming policy
   explicit and documented (NF-J7 lesson: map `variant == "high-contrast"` to
   `inputs.highContrast` or record the deliberate default).
4. Semantic severity surfaces consume `status.*`/`danger` tokens, never
   neutral or ad-hoc colors for warning/danger states (SF-4/SF-J2 lesson,
   generalized to all four apps).
5. Per-app theme default is documented; `qinda-dark` is the launch default
   (editor wiki line 109). The Settings scaffold currently consumes **no**
   tokens at all (`Main.qml` imports only QtQuick/Controls, default style) —
   acceptable for the existing DND route only as long as nobody claims it is
   QST-1 themed; its first real slice adopts interface §4 wholesale.

## 5. Accessibility and keyboard interface

The editor's assertion style is the contract (boundary-note rule 7; Juno §10):
explicit initial focus; complete tab chain with a documented focus-path
decision for any persistent banner/overlay (N-3/NF-J4); accessible
name/description on every interactive control; assertive announcements wired to
**transition-only** signals, never per-frame state (SF-J1 lesson); destructive
choices never default; every pointer gesture has a command path. Offscreen
focused tests assert accessible metadata and focus policy
(`tst_editor_window.cpp:72-74` precedent); live AT-bridge qualification stays
in the display matrix (harness page, line 360-362 division). Each app ships
this as focused tests, not as shared a11y code.

## 6. Session-participation and determinism interface (the new rule)

This is the only genuinely new durable choice, and it is one sentence:
**first-party apps are supervised, non-essential session clients with a
deterministic capture contract.**

1. The session supervisor's essential set stays exactly notification host +
   shell (`compositor-session.md:63-74`). Apps are **never** essential
   children: an editor crash must not tear down the desktop, and desktop
   teardown must still reap app processes. Launch happens through the
   supervisor's tokenized launcher in a non-essential role (or by the harness
   runner directly under the dev-session marker) so both lifetime directions
   stay owned.
2. Apps must boot and render honestly without Settings1 or any other service
   (typed degraded state; editor reads no settings at all; the Settings
   scaffold already warns and continues, `main.cpp:39-41`). The screenshot
   matrix must never depend on service availability.
3. Apps never touch the host session: they inherit the `qindaqt-dev-session`
   XDG/bus isolation and must not open their own host connections. Test input
   only ever arrives through the nested seat/development injector — never
   host uinput (testing-harness.md:8-15).
4. Deterministic capture family — the per-app diagnostic convention that
   generalizes the editor's CLI (ledger D-6; the File Manager/Terminal
   duplication trigger fires at their first slices, then it becomes a
   coding-practices paragraph, still not a module):
   - `--theme <id>` (+ `--theme-directory`): resolved validated theme.
   - `--check-theme`: resolve + derive + print identity + exit before any
     window (editor `main.cpp` / wiki lines 114-117 precedent).
   - `--report-startup`: milliseconds after first painted frame only.
   - `--screenshot <file>` (new, per app): render one settled, themed frame
     of the primary top-level, write PNG, exit 0; any failure exits non-zero
     with a bounded diagnostic. No animation clock in the captured frame;
     reduced-motion-safe. This gives the harness a toolkit-neutral capture
     path for any app (QML `QQuickWindow::grabWindow`, Widgets `QWidget::grab`)
     while the crew adds any compositor-side whole-output capture separately.

## 7. App-owned vs shared — the whole split

| Concern | Owner now | Shared extraction trigger (ledger) |
| --- | --- | --- |
| Document model, PTY session, directory model | App-private forever (no `DocumentStore` for files/terminal) | D-5: none visible |
| Consent flows, degraded banners, status copy | App-private; QML apps may adopt Controls S2 `StateCard`/`DegradedNotice` *after* integration | shared degraded components via Controls, not an app-shell module |
| Action tree / menubar | App-private (QMainWindow *is* the registry) | D-3 exporter, shell-lane, own ADR |
| Widgets QST→palette projection | Per-app adapter struct | D-1: second Widgets app start |
| QML scaffolding, route registry, breakpoints | Settings-app-owned | D-5: second QML content app |
| CLI diagnostic convention | Per-app, copied | D-6: File Manager/Terminal duplication → coding-practices paragraph |
| Desktop identity, app_id, window title, action names | Contract (this document) per app | frozen compatibility surface per app page |
| Scenario `applications` field, runner launch/teardown, capture seam, window-inventory assertions | Display crew (companion message) | n/a — harness property |
| Session supervisor essential set | session_supervisor owners; unchanged | n/a |

## 8. Executable acceptance fixtures (proposed names, display-crew to register)

Per ADR-0015's gate (boot, interact, screenshot, teardown; 1080p/WUXGA/1440p;
scale/theme/profile variants; confined synthetic input) and the harness's
determinism rules (`testing-harness.md:701-714`):

- **F-1 focused offscreen capture (per app):** `--screenshot` under the
  offscreen platform for all five built-in themes; assert exit 0, exact PNG
  dimensions = documented initial logical size × DPR, non-uniform pixels,
  no crash. Widgets precedent: `tests/apps/text_editor/`; QML precedent:
  `qindaqt.notification-surfaces-offscreen`.
- **F-2 theme resolution (per app):** `--check-theme` for all five themes
  plus one non-default installed-prefix row (NF-J6's `qinda-light` ask,
  generalized).
- **F-3 nested participation row (per app × scenario):** boot
  `qindaqt-dev-session --backend virtual --scenario <s> --execute`; launch the
  app through the scenario application declaration; assert via the compositor
  inventory a mapped normal window with the exact `app_id` and title; drive
  one keyboard action through the development input device; capture a
  screenshot (app `--screenshot` file and/or crew capture seam); deterministic
  teardown. Scenario set: `single-1080p`, `single-1080p-125`,
  `single-wuxga`, `single-1440p`; theme `qinda-dark` default plus one
  `qinda-light` row; one non-default profile row. Proposed selector family:
  `session.app-participation.<app>-<scenario>` (naming is the registrar's).
- **F-4 installed identity (per app):** staged install; desktop-file
  validation; `app_id` == desktop ID asserted from the live session; installed
  `--check-theme`.
- **F-5 lifetime isolation:** kill the app mid-session — desktop survives,
  shell/host unaffected; end the session — every app PID reaped (supervisor
  no-orphan pattern, `testing-harness.md:388-398` precedent).
- **F-6 screenshot baselines:** pinned fonts/wallpaper/locale/time/sample
  data; perceptual compare with documented AA tolerance; intentional changes
  need human review (harness determinism rules verbatim).

Rows F-1/F-2 belong to each app's focused suite (Linnea-owned for the editor;
extend her existing matrix). F-3–F-6 are harness-lane (companion crew
message). Nothing here widens the editor's S1 completion evidence; her resume
claim already forbids a nested-runtime claim for S1, and this contract does
not change that.

## 9. ADR recommendation

One new ADR, first-party-apps owner + session/harness owners as co-signers,
recording exactly two durable choices: (1) first-party apps are supervised
non-essential session clients (supervisor essential set unchanged); (2) the
four-command deterministic capture convention (`--theme` / `--check-theme` /
`--report-startup` / `--screenshot`) is the app-side participation contract
for the ADR-0015 matrix. Everything else in this document is consolidation of
already-accepted decisions and needs only wiki registration (an `apps/` page
section or a short `applications.md` page) when the first app adopts it.

## 10. Asks

- **Linnea:** nothing for the S1 repair lane — finish the resumed repairs and
  evidence exactly as claimed (`1787893613`). When you resume, one optional
  cheap add: `setDesktopFileName` parity check for the editor (already
  correct) and keep `--screenshot` in mind as the natural SF-5-adjacent
  large-document/probe sibling. No action required before handoff.
- **Juno:** reconcile this with your S3 proposal — the practical delta is that
  route registry/rail scaffolding stays in `settings_center` until a second
  QML app exists; your Q1–Q4 reconciliation already matches this.
- **Display crew:** see the companion message for the harness-side rows.

— Rowan Lee, 2026-08-28T05:14:50Z

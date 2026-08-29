# Nia Hart audit findings: Controls S2 questions 1–7

- **Timestamp:** 2026-08-27T23:44:30Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Mode:** read-only; nothing edited, built, run, or generated. Your
  uncommitted candidate is byte-for-byte intact.
- **Inspected identity:** `worker/controls-s2` HEAD
  `a083a20af14a2d7b9e954735a2d659c475a536b2`; uncommitted diff = 6 modified
  tracked files (+43 lines: wiki/module-boundaries, testing-harness, index,
  mkdocs.yml, both CMake registries) plus untracked
  `docs/wiki/shell/controls.md`, `src/controls/**` (15 files),
  `tests/controls/**` (13 files); no `tests/controls/baselines` yet.
- **Evidence class:** every item below is verified source fact unless marked
  *inference* or *missing runtime evidence*. No test has executed against the
  repaired sources; the six behavior repairs remain authored-only.

## Q1 FormRow — PASS with three bounded risks

Verified (`src/controls/qml/FormRow.qml`): all three children carry explicit
`Layout.row`/`Layout.column` in both modes — wide 2-column (label 0,0 /
editor 0,1 / error 1,1), compact 1-column (0,0 / 1,0 / 2,0) — so no
auto-placement collision is reachable; editor host gets `Layout.fillWidth` and
`childrenRect`-derived implicit size; label→editor→error order holds in both
modes; all three texts wrap (`Text.Wrap`) with mirrored alignment from
`control.mirrored`; the editor's Accessible name/description are written by two
`Binding` objects with `restoreMode: RestoreBinding`, leaving the editor's
native role/value intact; positive compact height is asserted by
`tst_controls_behavior.cpp:526` (`row.implicitHeight > field.implicitHeight`,
`field.width > 0`).

1. **Low (watch item, inference):** `editorHost.implicitWidth:
   childrenRect.width` (FormRow.qml:97-98) combined with the scene editors'
   `width: parent.width` (BehaviorScene.qml:89, ControlsGallery.qml:72,86) is
   the classic `childrenRect` pattern: implicit width depends on child width
   depends on host width assigned by the layout. It should converge (layout
   writes are outside implicit-size evaluation), but Qt's loop detector and
   first-pass sizing are only provable at runtime — watch stderr on the
   requalification run.
2. **Low (coverage gap):** no nonvisual assertion exercises wide mode
   (`width >= 480`); the only FormRow geometry test forces the 420 px narrow
   scene. Wide-mode cell truth currently rests on the not-yet-generated
   ordinary-width baselines.
3. **Low (consumer trap, source fact):** `required property Item editor` does
   not reparent. The supported pattern is declaring the editor as the row's
   child and binding `editor: <id>` (as both scenes and
   `tst_installed_controls.qml:11` do). `editor: C.TextField { }` inline would
   create an unparented, invisible editor with correct accessible bindings.
   `docs/wiki/shell/controls.md:53` says "required" but not the declared-inside
   requirement.

## Q2 ThemeCard hostile totality — PASS

Verified (`src/controls/qml/ThemeCard.qml`): every function returns a strict
boolean (`colorComponent`/`colorRole`/`completePreview` are all-guard chains of
comparisons), so `previewValid` is always Boolean; guards check
null/`typeof` **before** any property read, so null, non-object (number,
string), array, missing-group, wrong-typed, NaN, ±inf, and out-of-range shapes
cannot reach unsafe indexing or throw; invalid maps take one explicit
unavailable branch (`role()` returns `Tokens.status.warning.foreground`, never
a per-role fallback — AGENT-GUARD at ThemeCard.qml:51-53); `enabled:
available && !previewUnavailable` keeps caller `available` intact;
`tst_controls_behavior.cpp:427-484` proves partial/missing-group/wrong-typed/
NaN/inf/out-of-range rejection and — key — accepts a complete map built from
real `facade->bg()/accent()/fg()/outline()` QColor maps, confirming real QST
colors pass the r/g/b/a numeric contract.

- **Low (coverage gap):** non-object `previewTokens` (string/number/array) is
  handled in source but never asserted; and no test installs a Qt message
  handler, so "no transient binding warnings" is source-argued
  (*inference*), not runtime-observed.

## Q3 StateCard accessibility — PASS

Verified (`src/controls/qml/StateCard.qml`): `announceStatus()` derives
`isAlert` directly from `status` (AGENT-GUARD :57-61), never from the `alert`
binding; Information/Success/Busy → `Accessible.Polite`, Warning/Error →
`Accessible.Assertive`; the exact tuple (message, status, politeness) goes to
`Accessible.announce` and to `accessibilityAnnouncementRequested` from the same
locals; politeness values come from the public QML properties
`politeAnnouncement`/`assertiveAnnouncement` (:28-29) read from
`Accessible.Polite`/`Accessible.Assertive`, and the test compares the signal
against those properties (tst_controls_behavior.cpp:364-365) — no scoped C++
enum numeric identity assumed; static Busy text is complete (name "%1, busy",
description "Busy. %1", asserted at :168-171); creation-time announcements are
correctly suppressed until `accessibilityReady`.

- **Low (coverage gap):** no assertion that `assertiveAnnouncement !==
  politeAnnouncement`; if both Qt values ever collapsed, all five
  `requireTransition` checks would still pass.
- **Low (coverage gap):** the announcement prefix (status name) and message
  are asserted, but the title segment of `"%1: %2 — %3"` is never asserted.

## Q4 TextField sizing and lint — PASS statically

Verified (`src/controls/qml/TextField.qml:16-18`):
`Math.max(40, contentItem.implicitHeight + topPadding + bottomPadding)` —
`contentItem` is declared `QQuickItem*`, whose `implicitHeight` exists, so
qmllint's type resolution accepts it (the rejected `implicit-content` member is
gone); the expression is one-way (font/point-size derived) with constant token
paddings and a 40 logical-px floor, so it stays positive and loop-free at any
`Tokens.type.body` text scale. **Missing runtime evidence:** the repaired lint
target has not re-run, so lint-clean remains unproven.

## Q5 Keyboard semantics — PASS

Verified: Space activation asserted for Button (:243-244), CheckBox (:247-248),
Switch (:251-252), ThemeCard (:269-270), StateCard action (:282-283), and
DegradedNotice retry (:291-292); slider uses Right arrow (:256); TextField uses
per-character `keyClick` (:261-266); **no `Key_Return`/`Key_Enter` exists
anywhere under `tests/controls/`** (grep-verified), matching the checkpoint's
supported-activation finding. Busy suppression is proven before/after: Space on
a focused busy button yields zero clicks, then `available` round-trips without
loss and activation succeeds (:295-314) — caller state is preserved because
`enabled: available && !busy` never writes `available`
(Button.qml:10,19).

- **Low (coverage gap):** "reachable" is satisfied via `forceActiveFocus`; no
  test presses Tab to prove real traversal. Native traversal plus StrongFocus
  makes this low-risk, but the wiki's "reachable with a keyboard" currently
  rests on inference for ordering.

## Q6 Five-theme and scale visuals — structure PASS, one evidence gap

Verified (`tests/controls/tst_controls_visual.cpp`, `CMakeLists.txt:60-75`):
100% defines 5 themes × compact(420)/ordinary(720)/large(1080) = 15 rows;
125%/150% define 5 ordinary rows each = 25 total; each process verifies
`|devicePixelRatio − requested| < 0.01` (:121) and captured pixel dimensions
exactly (`qRound(logical × scale)`, :124-126); fonts substitute
Inter/JetBrains Mono → Noto Sans with C locale (control_test_support.cpp:104-114)
and Noto presence is asserted (:61-62); software backend + offscreen +
PassThrough rounding come from the test ENVIRONMENT; baseline names live in
per-scale directories (`100/`,`125/`,`150/`) and are collision-free; the
tolerance (max channel ≤ 8 AND ≤0.1% pixels) is small and reviewable; all
animated properties (knob x, fill width, hover overlay, borders) start at their
final values and nothing mutates after show, so animation nondeterminism is
structurally avoided (*inference*, since no run exists yet).

- **Medium (fixture evidence gap):** the visual fixture is
  `ControlsGallery.qml` only, which contains no error, disabled, or busy
  presentation — `Tokens.danger.*` borders/backgrounds, `fg.disabled`, and
  "Working…" are never rendered in any of the 25 rows, and no behavior test
  asserts those colors either (only StateCard's role/announcement and the
  `error` property state are checked). Error/busy/disabled *appearance* is
  therefore unreviewable under the current matrix. If that is intentional S2
  scope, the wiki's fixture sentence could say so; otherwise consider one
  degraded/error row before baseline generation.
- **Expected, not a defect:** `tests/controls/baselines` does not exist, so
  the three visual CTests will fail "missing baseline" until generation — the
  wiki's `^qindaqt\.controls-` selector cannot pass today (build dir in wiki is
  the established generic `build/dev` convention; actual focused work used
  `build/controls-debug`).

## Q7 PSS and installed import — PASS

Verified (`tests/controls/measure_pss.py`, `bare_memory_probe.cpp`,
`controls_memory_probe.cpp`): both probes are matched on offscreen platform,
software backend, QQuickView 720×840, a 200 ms READY handshake printing the
exact application PID, and the measurement reads `/proc/<pid>/smaps_rollup`
from that PID; PSS sampling is 5 × 50 ms with per-process median, 3
bare/controls pairs, median-of-3 delta reported with `"threshold": null`; cleanup
terminates → waits 3 s → kills → waits. The installed consumer
(`run_installed_controls_consumer.cmake`) refuses prefixes outside the build
tree, `REMOVE_RECURSE`s its own staged prefix, installs with the current
configuration, asserts `qmldir` + `qmltypes` for Controls and `qmldir` for
Tokens, then runs qmltestrunner with only `-import <installed qml root>` and
`QML_DISABLE_DISK_CACHE=1`; the consumer QML resolves 4 representative types
from `QindaQt.Controls 1.0` and would fail on any fallback.

- **Low (hardening nit):** ambient `QML_IMPORT_PATH`/`QML2_IMPORT_PATH` from
  the invoking shell are not unset; `addImportPath` order still favors the
  `-import` root, so provenance holds, but defensively clearing them would make
  the proof airtight.

## Additional flags

- **Medium (wiki claim vs implementation):**
  `docs/wiki/shell/controls.md:72-73` states "A color change alone never
  conveys busy, error, required, degraded, checked, selected, or disabled
  meaning." Inside the module, `Button.error` and `TextField.error` change only
  the border color (Button.qml:49, TextField.qml:38-39); no accessible
  name/description/state change accompanies them unless the caller supplies
  one. Either soften the wiki sentence for caller-supplied error semantics or
  add an accessible error indication to those two components.
- **Low (wiki wording):** `controls.md:56` says the unavailable ThemeCard
  preview is "announced"; it is conveyed by an `Accessible.description` change
  only — no `Accessible.announce` call exists in ThemeCard.qml.
- **Clean:** policy-equivalent greps confirm no hex literals, no
  `qinda-(light|dusk|dark|high-contrast|macos)`, no `sourceThemeId`, no
  forbidden imports in `src/controls/qml/`; the only QindaQt import anywhere is
  `QindaQt.Tokens 1.0` (no themes QML module exists to escape through; src QML
  URIs are Tokens/Shell/Shell.Runtime only). `git diff --check` exit 0.
  All production QML ≤ 192 lines; every role/key the controls use
  (`bg.base/raised/highest`, `fg.default/muted/disabled`, `accent.default/fg/
  subtle`, `state.pressed/hover`, `focus.ring`, `outline.strong/divider`,
  `status.*.background/foreground`, `danger.default/fg`, `radius.s/m/l`,
  `space["1".."6"]`, `type.fontFamily/body/caption/title`, `motion.short`)
  exists in the QST derivation; theme data files for all five built-ins exist
  under `data/themes/`. AGENT markers in FormRow/ThemeCard/StateCard/test
  support/CMake are current and accurate.
- **Headline caveat (missing runtime evidence):** the focused requalification,
  qmllint re-run, baseline generation/review, installed import, PSS run,
  Release/broad, and docs gates are all unexecuted against these repaired
  sources. Nothing above predicts a failure, but none of it is yet evidence.

No scope outside S2 is proposed.

# Juno Park handoff: executable native-app visual/interaction acceptance matrix (Settings Center, Text Editor, File Manager, Terminal)

- Timestamp: 2026-08-28T05:13:17Z
- From: Juno Park, native applications design engineer (read-only; no product
  edits, no Git, no build, no runtime, no GUI)
- To: Linnea Marsh (First-party apps implementer), Rowan Lee (AppShell
  experience architect); manager for lane allocation
- Claim: `1787893938-juno-park-visual-matrix-claim.md`
- Method: design synthesis over read-only inspection of the integrated
  Settings Center (`/home/cabewse/work_SPaC3/container-wm-workers/qst1-manager-integration/src/apps/settings_center/` + its test), the Text Editor S1
  contract (`text-editor-s1/docs/wiki/apps/text-editor.md`), the Controls/QST
  authority docs (`controls-s2/docs/wiki/`), ADR-0015/0013/0021/0022, and the
  testing-harness display matrix. Nothing was executed; every runnable row
  below cites the gate that would run it.

## 0. Verdict and shape

The four apps are at four different maturity points, so one flat matrix would
lie. The matrix is therefore **three tiers per app**, with tier ownership and
blockers explicit:

- **Tier A — Offscreen visual rows** (25 per app, Controls-parity): theme ×
  width × scale screenshot baselines under the ADR-0021 runner.
- **Tier B — Offscreen interaction/accessibility rows** (per app, named
  below): keyboard-only, focus, accessible names/roles/states/announcements,
  degraded states, accessibility-input mapping.
- **Tier C — Nested-session rows** (7 per app, harness-standard): real
  launch → map → interact → capture → teardown inside the private virtual
  desktop at the ADR-0015 resolutions/scales.

Current runnability: Tier B exists partially for Settings Center and Text
Editor; Tier A exists only for the Controls gallery (not yet for any app);
Tier C for apps waits on the ADR-0015 whole-desktop workflow, which the
harness correctly records as not yet built. This deliverable makes those gaps
nameable and bounded instead of implicit.

## 1. Normative anchors (rows inherit these; none restated per app)

- **ADR-0015** (Accepted, qst1-manager-integration): qualification gate =
  bootable, interactable nested desktop; 1920×1080, 1920×1200, 2560×1440;
  representative DPI/theme/profile variants; screenshots and input traces
  only from the private nested environment; deterministic teardown
  (`adr/0015-qualify-function-before-resource-refinement.md:24-28,40-45`).
- **Testing harness "Required display matrix"**:
  1080p at 100/125/150%; WUXGA at 100% (portrait stays a shell/output
  concern — apps see it only as a resized work area, covered logically by
  narrow/wide rows); 1440p at 100/125/150%
  (`development/testing-harness.md:673-679`).
- **Determinism and acceptance**: backends must report which scenario
  declarations they consumed; baselines pin fonts, wallpaper, locale, time,
  animation clock, and sample data; perceptual comparison allows documented
  antialiasing tolerance; intentional baseline changes require human review
  (`testing-harness.md:696-703`).
- **ADR-0021**: one named CTest + one fresh process + exactly one validated
  QtTest data selector per visual row; DPR and pixel-dimension assertions
  before compare; capture only after the fixture's published QST transition
  duration; reviewed PNG baselines; serial by default
  (`adr/0021-isolate-controls-visual-rows.md:24-37`).
- **QST-1**: five built-ins (Qinda Light, Dusk, Dark, High Contrast, macOS);
  caller inputs `textScale` (0.5–3.0), `reducedMotion`, `reducedTransparency`,
  `highContrast`; composition must publish one complete generation before
  token-dependent construction; WCAG pair scope including 7:1 High Contrast
  (`architecture/design-tokens.md:71-83,133-152`).
- **Controls S2 visual precedent** (the fixture shape every app copies):
  five themes × compact/ordinary/large widths at 100%, plus all five
  ordinary-width themes at truthful 125%/150% DPR = 25 rows; named font
  substitutions `Inter→Noto Sans`, `JetBrains Mono→Noto Sans Mono`; C locale;
  offscreen software rendering (`shell/controls.md:107-123`).
- **ADR-0022 + Rowan's rules**: window-owned named standard-key actions;
  ownership trees not singletons; caller-resolved themes via per-app
  projection; typed honest degraded states; statics-only file dialogs;
  per-app keyboard/a11y assertion offscreen
  (`first-party-native-apps/1787892238-rowan-lee-appshell-boundary-recommendation.md:141-199`).

## 2. Reusable fixtures (build once, all four apps consume)

These are harness/test-tree assets, **not product modules** — consistent with
Rowan's "no AppShell umbrella" rule; the natural home is generalized
utilities beside `tests/controls`' existing wrapper.

- **F-1 Visual row runner (ADR-0021-generic).** Today the 25-row wrapper is
  QML/Qt Quick-specific. Extraction ask: a runner that takes (app-under-test
  or gallery, window logical size, DPR, theme, data selector) and enforces
  fresh-process isolation, selector validation, DPR+dimension pre-asserts,
  and QST-motion-settle capture — with **two capture backends**: Qt Quick
  offscreen (exists) and **QWidget grab mode** (new; needed by the editor).
  Harness-side, so no product dependency is created.
- **F-2 Environment pin set.** C locale, offscreen platform, software
  rendering, the two named font substitutions, pinned sample
  documents/directories/prompt content, pinned animation clock. One
  definition, imported by every row.
- **F-3 Theme fixture loop.** For each app: iterate all five
  `data/themes/*.json` through the app's own theme entry (validated
  `ThemeSpec` → `derive()` → publish) and assert `ok()`/complete generation
  before any window exists. Directly reuses NF-J6's proposed editor loop.
- **F-4 Accessibility-input fixture.** Caller-input arms: `highContrast`
  (must be armed for `variant: "high-contrast"` — NF-J7's open policy),
  `reducedMotion`, `reducedTransparency`, `textScale` 1.0/1.5. Each app
  declares its mapping once; rows assert the mapped effect (capped duration,
  opaque colors, strengthened focus/outline, scaled type ramp).
- **F-5 Keyboard driver.** Offscreen `QTest::keyClick` sequences plus a
  shared assertion helper that enumerates the window's persistent
  window-context actions and verifies object-name stability and
  standard-shortcut identity (Rowan rule 1, already editor-proven at
  `tst_editor_window.cpp`).
- **F-6 A11y tree/announcement probe.** QAccessible interface queries
  (names, roles, states, focus chain) and announcement capture via each
  component's published announcement surface (Controls'
  `accessibilityAnnouncementRequested` pattern; editor's announcement path
  after SF-J1). Never a live AT bridge — that stays with the display matrix
  per the harness.
- **F-7 Perceptual comparator.** Decode → assert exact dimensions →
  compare per §4; baseline directories `tests/apps/<app>/baselines/`;
  baseline regeneration is a reviewed, intentional act (harness rule), and
  threshold loosening is never a substitute for it.
- **F-8 Installed-prefix probe convention.** `--check-theme` /
  `--report-startup` style pre-window diagnostics run against the staged
  prefix (editor precedent, `docs/wiki/apps/text-editor.md:114-120`).
  Becomes a coding-practices paragraph, not a module, when the second app
  duplicates it — Rowan D-6.
- **F-9 PSS/settle sampler.** Five-settled-sample median PSS with exact PIDs
  (editor 64 MiB gate convention; Controls PSS pairing) — used by perf rows,
  no cross-machine threshold claims.
- **F-10 Scenario pack (Tier C).** Named dev-session scenarios:
  `app-1080p-100/125/150`, `app-wuxga-100`, `app-1440p-100/150/125` — each
  declaring theme, app under test, private buses, and fixture state, with
  the backend's consumed-declaration report attached to the row result
  (harness §Determinism; ADR-0015 evidence duty).

## 3. Tier definitions and the per-app matrix

Tier A rows per app (25, Controls-parity): `narrow / ordinary / wide` logical
widths × five themes at 100% (15), ordinary width × five themes at 125% (5)
and 150% (5). Tier C rows per app (7): 1080p×{100,125,150}, WUXGA×100,
1440p×{100,125,150}. Tier B rows are named per app below. Per-change runs use
a focused pairwise subset; the complete matrix is a release gate (harness
rule).

### 3.1 Settings Center — `qindaqt-settings` (exists: one Notifications route)

State of the tree: `Main.qml` is an `ApplicationWindow` 560×420, min
420×320, importing only `QtQuick`/`QtQuick.Controls`
(`src/apps/settings_center/Main.qml:5-21`); `main.cpp` builds the Settings1
client + `DoNotDisturbController` on the session bus and loads QML — no
theme resolution anywhere (`main.cpp:33-53`); the page asserts initial
focus, an explicit dynamic KeyNavigation chain across conflict/Retry/Close,
and AlertMessage status roles
(`NotificationsPage.qml:12,29-47,49-71,75-112`); offscreen tests already
cover load/ready/saving, conflict and unavailable focus chains, retry
recovery, and rebaselined save failure
(`tests/apps/settings_center/tst_settingsapp.qml:48-141`).

App-specific rows (Tier B, extending the existing four tests):

- **SC-B1 Loading→ready→saving**: toggle disabled until `canToggle`; status
  text transitions truthful (already covered; keep as the regression row).
- **SC-B2 Conflict route**: Apply-my-choice appears, focus chain
  switch→action→close, status is AlertMessage (covered; keep).
- **SC-B3 Unavailable→Retry→recovery** (covered; keep).
- **SC-B4 Save-failure after rebaseline stays visible** (covered; keep).
- **SC-B5 Announcement semantics**: status becoming AlertMessage emits
  exactly one polite announcement per transition, coalesced per event turn
  (Controls StateCard convention) — **new row, currently unasserted**.
- **SC-B6 Escape/close**: close route never persists an unconfirmed toggle;
  window close == controller close-request path.
- **SC-B7 Settings1 round-trip** (private-bus fixture, offscreen): set →
  persisted → fresh client reflects value; service loss mid-flight renders
  the unavailable state.
- **SC-B8 Localization wrap**: long localized status/labels wrap and grow
  height at narrow width (harness flags translated layouts as unqualified
  elsewhere — this row is offscreen text-wrap only, no live-AT claim).

App-specific narrow/wide: narrow = 420×320 (min; status wrap + button row
order), ordinary = 560×420 (default), wide = 960×640 (single column
retained; center grows the page, not a second column — pin this as the
intended S1-layout answer until real navigation lands).

**Concrete findings on the current integrated tree — route to Rowan (design)
and the future Settings Center slice owner:**

- **SC-1 — No QST-1 boundary exists in the app.** No `QindaQt.Tokens` import,
  no `ThemeSpec` resolution, no `--theme`, no publish-before-construct;
  default Qt Quick Controls styling renders today
  (`Main.qml:2-3`, `main.cpp:43-53`). Consequence: every Tier A theme row
  and the high-contrast/textScale rows are **unrunnable for this app** until
  the app adopts caller-resolved QST-1. This is exactly Rowan's D-1 trigger
  ("Settings Center consumes QST-1 → extract the smallest seam
  inside/near `design_tokens`"). Minimal shape: resolve theme in `main.cpp`,
  publish via the Tokens facade before `engine.load…`, add `--theme` +
  `--check-theme` for F-8 parity.
- **SC-2 — Hard-coded type defeats accessibility scaling.** Heading uses
  `font.pixelSize: 24` (`NotificationsPage.qml:22`) instead of a QST type
  role; once SC-1 lands this must come from the ramp or `textScale` rows
  (F-4) will show a frozen heading. Trivial with SC-1; filing separately so
  it survives SC-1's larger diff.
- **SC-3 — Positive note worth keeping**: the injected-controller test
  double (`tst_settingsapp.qml:11-25`) is the exact seam F-6/Tier B rows
  need; no fixture change required for the app's offscreen rows.

### 3.2 Text Editor — `qindaqt-editor` (S1 candidate in Linnea's repair)

Contract source: `docs/wiki/apps/text-editor.md` (action table 20-36,
theme/a11y boundary 104-126, gates 128-148); my findings SF-J1/J2,
NF-J3–J8 (`first-party-native-apps/1787892960-…handoff.md`) are already in
Linnea's triage (`1787893613`). This matrix converts them into rows —
no new code findings.

- **Tier A (blocked on F-1's Widgets capture mode, then runnable)**:
  narrow = 420×320 (banner + wrap + NF-J3 focus return), ordinary = 920×680
  (default), wide = 1920 logical (maximized content width); five themes;
  scale rows at ordinary. Prerequisites already filed: NF-J6 (five-theme
  adapter loop = F-3), SF-J2 (severity tokens so `Changed`/`Missing`/
  `Unreadable` baselines differ truthfully, not color-only — pair with a
  glyph/icon or border treatment), NF-J7 (high-contrast input mapping = F-4).
  With banner visible in every theme row so severity rendering is reviewed
  (the exact gap Nia Hart caught in the Controls fixture).
- **Tier B**: B1 keyboard action inventory (F-5; exists), B2 consent-dialog
  defaults (destructive never default; static, exists), B3 announcement
  wiring post-SF-J1 (transition-only via `externalStateChanged`), B4 focus
  return post-NF-J3, B5 dirty title `[*]`/Save enablement (exists), B6
  large-document perf row — Rowan SF-5: 8–32 MiB open+save under F-9;
  **belongs to the perf gate, not the visual matrix**.
- **Tier C**: launch → map at each of the 7 resolution/scale rows → type
  via isolated input → dirty marker + save → capture decorated window →
  clean teardown. Gates stay app-specific: 400 ms first paint, 64 MiB PSS
  (wiki 145-148) recorded per row via F-9.

### 3.3 File Manager — absent (QQ-006.07). Contract rows for the slice

No code exists; these rows are acceptance criteria to build against, so the
slice lands matrix-ready (same move that made the editor reviewable on day
one).

- **Tier A**: same 25-row shape; gallery must include selection highlight,
  rename-in-place editor, and an unreadable-directory degraded state — the
  Nia-Hart-lesson: state appearances must be *in* the baselines, not
  asserted only behaviorally.
- **Tier B (app-specific keyboard/a11y)**: arrow/Space/Ctrl+Space selection;
  `F2` inline rename commit/cancel with accessible name transitions;
  Back/Forward/Up as standard-key window actions (Rowan rule 1); cut/copy/
  paste with a replace-consent dialog whose destructive default is No
  (editor-consent parity); sidebar places reachable in the explicit tab
  cycle; unreadable/unavailable directory renders a typed, actionable
  degraded state (Rowan rule 6) — no document model, no dirty concept
  (deliberately unlike the editor).
- **App-specific boundaries to respect**: first QML/Controls consumer —
  consuming `StateCard`/`DegradedNotice` here is the real D-1 demand proof;
  file dialogs statics-only (Rowan rule 5); directory model + trash policy
  stay app-private (D-5: no shared "document framework").
- **Tier C**: 7 rows; interactions = navigate, select, rename, cancel a
  destructive consent via keyboard.

### 3.4 Terminal — absent (QQ-006.08). Contract rows for the slice

- **Tier A**: same 25-row shape with a pinned deterministic prompt/session
  transcript; monospace grid metrics from the **substituted**
  `Noto Sans Mono` family (F-2 — the substitution is load-bearing here, so
  terminal baselines must be generated under the same named substitution,
  never host `JetBrains Mono`); cursor rendered blink-disabled or
  phase-locked in captures; QST fg/bg/selection/accent colors in every
  theme row (fg.default-on-bg.base contrast is already guaranteed by the
  QST pair gate — the screenshot row verifies rendering, not contrast).
- **Tier B (app-specific)**: standard shortcuts `Ctrl+Shift+C/V` (copy /
  paste) as window actions; selection via keyboard per the decided policy;
  scrollback navigation; **reflow on narrow/wide is a decision to pin before
  baselines exist** (reflow vs. preserve-then-rewrap) — flagging now so it
  is not silently baked into first baselines; PTY spawn failure renders the
  Controls degraded state (typed reason + retry), never a fake prompt
  (Rowan rule 6); accessible identity = window name + terminal role +
  output-status announcements; full text streaming to AT stays out of S1
  scope — declare that in the wiki page up front.
- **App-specific boundaries**: one process owns one PTY session (editor's
  one-window-one-document shape, Rowan rule 2); no `DocumentStore`
  (D-5); scrollback memory recorded via F-9 with **no threshold** —
  ADR-0015's 1,024 MiB aggregate ceiling is the only bound until measured
  profiles drive tighter budgets.
- **Tier C**: 7 rows; interactions = type via isolated input, resize
  narrow→wide, copy/paste round trip, teardown with child PTY cleanup
  proven (session-supervisor precedent).

## 4. Screenshot tolerance policy (proposed defaults — reviewer-owned numbers)

All rows, all tiers:

- **T-1 Capture preconditions (mandatory, fail-closed)**: exact pixel
  dimensions == logical size × DPR asserted before compare; C locale;
  F-2 substitutions and pinned sample data/clock; capture only after the
  app's published QST transition settles (ADR-0021 boundary; reduced-motion
  rows assert the capped duration instead); one process per row.
- **T-2 Comparison budget**: exact match first. Where perceptual mode is
  enabled: ≤0.1% of pixels may differ with per-channel delta >2/255
  (antialiasing fringe), and **zero** differing pixels within a
  2-logical-pixel window border band (protects focus ring/outline
  correctness, the highest-risk regression band). These two numbers are
  proposals for the first baseline review to accept or tighten — never
  loosen.
- **T-3 Reduced-transparency rows** compare against their own flattened
  baselines (all-alpha-255, so exactness is verifiable), not the
  translucent set.
- **T-4 Font substitution differences are environment determinism**, not
  baseline content (ADR-0021/Controls convention); changing either
  installed family still requires baseline review.
- **T-5 Intentional visual change = reviewed baseline regeneration**;
  threshold adjustment is never an accepted substitute (harness
  `testing-harness.md:702-703`).

## 5. Reusable vs app-specific — the separation rule

Reusable (fixture, build once): row runner + isolation, environment pins,
theme loop, accessibility-input arms, keyboard inventory helper, a11y probe,
comparator + baseline convention, installed-probe convention, PSS sampler,
scenario pack.

App-specific (never migrate): document policy/32 MiB (editor), PTY/reflow/
scrollback (terminal), directory model/rename/trash policy (file manager),
Settings1 route/controller composition (settings center), consent texts and
dialog flows, per-app minimum/ordinary/wide sizes, severity semantics of
each degraded state, desktop identity and CLI surface.

Litmus test (Rowan's, adopted): if a future app could copy the pattern
without a single shared header, it is app-specific; if two apps need the
*same code* (not the same shape), extract the one smallest seam behind an
ADR. On current evidence only F-1..F-10 pass that test, and all of them live
in the test tree.

## 6. Routing and requested actions

**For Linnea** (no change to your triage; adds executable targets):
1. After SF-J2 lands, Tier A editor baselines become meaningful — the
   banner must appear in every theme row so severity is reviewed
   pixels-first.
2. NF-J6 = F-3 editor instance; NF-J7 = one F-4 row asserting the
   high-contrast mapping decision you document.
3. SF-5's large-document cost row belongs in the perf gate (F-9), not the
   visual matrix.
4. Narrow/wide editor rows pin 420×320/920×680/1920-logical as above;
   NF-J3's focus return is a Tier B assertion.

**For Rowan** (design decisions your ledger predicted):
1. SC-1/SC-2 above are the D-1 trigger arriving early, via the Settings
   Center instead of the file manager — recommend the smallest seam inside
   `design_tokens` per your D-1 wording.
2. F-1's dual capture backend (QML + Widgets) is a test-tree utility ask;
   confirm it does not violate your no-umbrella rule (I believe it does
   not — no product code, no shared presentation seam).
3. D-2 (window-geometry/session contract) blocks any narrow/wide row from
   asserting *restore* behavior; propose deciding it before the second app
   lands.
4. Terminal reflow policy (§3.4) needs your ruling before first baselines.
5. D-6 is about to trigger: Settings Center will want `--check-theme`
   parity the moment SC-1 lands.

**For the manager**: Tier C rows consume the serialized private-runtime
lane and the not-yet-built ADR-0015 whole-desktop workflow; they are scoped
so the lane runs 4 apps × 7 rows in one session pack (F-10) once it exists.

Coverage envelope at release: 4 apps × (25 Tier A + 8–10 Tier B + 7 Tier C)
≈ 160–168 named rows; per-change focused subsets per the harness rule.

## 7. Caveats

Static design work only; I executed nothing and no gate above can be
claimed passing. Settings Center findings are against
`qst1-manager-integration` (integrated manager tree); the editor references
are against Linnea's `text-editor-s1` candidate at `94e8407` and my filed
handoff. Tier C remains blocked on capabilities the harness correctly
records as pending (input record/replay, topology application, app
scenarios). Numbers in T-2 are proposals requiring first-baseline review.

— Juno Park, 2026-08-28T05:13:17Z

# Celeste Ward findings: Controls S2 public API/docs consumer audit

- **Timestamp:** 2026-08-27T23:52:00Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Mode:** read-only; nothing edited, built, run, or generated. Your
  uncommitted candidate is untouched.
- **Inspected identity:** `worker/controls-s2` HEAD
  `a083a20af14a2d7b9e954735a2d659c475a536b2`, same uncommitted diff Nia Hart
  inspected. I read all 14 production QML files, `src/controls/CMakeLists.txt`,
  the prior build's generated `qmldir`/`qindaqt_controls.qmltypes` under
  `build/controls-debug/qml/QindaQt/Controls/` (stale artifact from before the
  six behavior repairs, cited only for type-registration shape, not behavior),
  `docs/wiki/shell/controls.md`, `module-boundaries.md`, `testing-harness.md`,
  `index.md`, `mkdocs.yml`, and the S2/S3 outcome assignments. I did not
  re-audit Nia's geometry, hostile-map implementation, key-event tests,
  visual matrix, PSS math, or installed-stage deletion logic; her two mediums
  and seven lows (`1787874240`) stand as she reported them.

## Q1 — 14 type names/version/install: PASS, stable and discoverable

`src/controls/CMakeLists.txt:14-28` `QML_FILES` lists exactly 14 files; the
generated `qmldir` registers exactly the same 14 names at version `1.0`
(`Button` … `TokenSwatch`); `docs/wiki/shell/controls.md`'s component table
lists the identical 14. Install rule (`CMakeLists.txt:48-60`) places the
backing library, plugin, `qmldir`, and `qindaqt_controls.qmltypes` together
under `${QT6_INSTALL_QML}/QindaQt/Controls`, matching the wiki's stated
install-prefix consumption story and Nia's installed-consumer evidence.
`qindaqt_controls.qmltypes` is an empty `Module {}` — correct and expected for
a pure-QML module with zero `QML_ELEMENT`-registered C++ types (compare
`design_tokens`'s non-empty qmltypes for its C++ `TokenFacade` singleton); this
is not a defect.

Ownership/lifetime/threading/error/compatibility claims in "Consumption and
ownership" are truthful against the read sources: no C++ file exists under
`src/controls` at all (pure QML), so "normal visual-parent lifetime and
GUI-thread affinity," "no persistence, asynchronous transport, platform
object, or service authority," and "error surface is visual/accessible state"
all hold structurally, not just by claim.

## Q2 — naming coherence and second authority: 4 findings

1. **Medium — undocumented public property, `ThemeCard.available`
   (`src/controls/qml/ThemeCard.qml:15`).** `available` exists as a
   caller-owned capability input on ThemeCard exactly parallel to
   `Button.available`, and `enabled: available && !previewUnavailable`
   (`:65`) depends on it. `docs/wiki/shell/controls.md`'s ThemeCard row
   (line 56) documents preview-hostility disabling but never mentions
   `available`; the only `available` reference in the whole page is in the
   Button row (line 45). A consumer cannot discover from the docs that
   ThemeCard has a caller-settable availability input independent of preview
   validity.

2. **Medium — `enabled` remains a live second authority over `available` on
   Button and ThemeCard.** `Button.qml:19` declares `enabled: available &&
   !busy`; `ThemeCard.qml:65` declares `enabled: available &&
   !previewUnavailable`. Both are ordinary bindings on inherited, publicly
   writable `Item.enabled`. Nothing prevents (and QML gives no diagnostic
   for) a consumer instance overriding it, e.g. `Qinda.Button { enabled:
   someExpr }` or `Qinda.ThemeCard { enabled: someExpr }`, which silently
   replaces the component's own binding. That breaks exactly the guarantees
   the wiki documents as load-bearing: "busy always suppresses pointer,
   keyboard, and accessible activation" (Button) and hostile/partial preview
   maps "disable selection" so "roles never fall back individually into a
   hybrid of themes" (ThemeCard, a stated safety property). Neither the wiki
   nor the QML declares `enabled` non-overridable guidance or documents this
   trap; the ordinary, idiomatic Qt Quick Controls habit of setting `enabled`
   directly is exactly the footgun. (CheckBox/Switch/Slider/TextField have no
   `available` and no busy concept, so this risk is scoped to Button and
   ThemeCard only.)

3. **Medium — DegradedNotice inherits StateCard's entire public surface,
   producing two parallel naming systems (`src/controls/qml/
   DegradedNotice.qml`).** DegradedNotice is declared as `StateCard { … }`
   (QML inheritance, not composition), so every StateCard property/signal —
   `status`, `title`, `message`, `actionText`, `busy`, `error`, `alert`,
   `politeAnnouncement`/`assertiveAnnouncement`,
   `accessibilityAnnouncementRequested`, `actionTriggered`, etc. — remains
   public and settable on a DegradedNotice instance, alongside DegradedNotice's
   own `reason`/`retryText`/`retryRequested()`. `message: reason` and
   `actionText: retryText` (`:14-15`) are plain one-way bindings a consumer
   can silently override by setting `.message`/`.actionText` directly, at
   which point `reason`/`retryText` stop reflecting what is actually
   displayed with no diagnostic. `status: StateCard.Warning` (`:12`) is
   likewise a plain overridable binding: nothing stops
   `Qinda.DegradedNotice { status: StateCard.Busy }`, which would contradict
   the wiki's description of DegradedNotice as "an explicit unavailable
   capability alert" and its "never decides whether a service is available"
   framing. The wiki does not mention that DegradedNotice is a StateCard
   subtype or name any of this inherited surface.

4. **Medium — FormRow silently supersedes an editor's own naming properties,
   undocumented (`src/controls/qml/FormRow.qml:35-49` vs. e.g.
   `TextField.qml:9-10`).** TextField, CheckBox, Switch, and Slider each
   expose their own public `accessibleName`/`accessibleDescription`
   properties as their normal naming API. FormRow's two `Binding` elements
   (`restoreMode: RestoreBinding`) take over `editor.Accessible.name` and
   `.description` for as long as `editor !== null` — i.e., for the entire
   life of a required, non-nullable property — so any value a consumer sets
   on the wrapped editor's own `accessibleName`/`accessibleDescription`
   becomes completely inert while inside a FormRow, with no warning. The
   wiki's FormRow row says the row "preserv[es the editor's] native role and
   value interface," which is true, but does not say the editor's own naming
   properties are superseded; a reader could reasonably expect
   `TextField.accessibleName` to still take effect. Confirmed untested: the
   only FormRow fixture (`tests/controls/qml/BehaviorScene.qml:88-94`) leaves
   the wrapped TextField's `accessibleName`/`accessibleDescription` unset, so
   this conflict has never been exercised.

`StateCard` statuses/announcement mapping and `FocusRing`'s required-control
contract are coherently named with no second-authority issue: `politeness`,
`status`, `alert`, `busy`, `error` are single-source, readonly-derived where
it matters (Nia's Q3 already verified the runtime derivation order), and
`FocusRing.control` is a `required` property with no competing input. I have
no findings there beyond Nia's existing coverage.

## Q3 — links/nav/current-truth: no defects found

`mkdocs.yml` nav, `docs/wiki/index.md`, `module-boundaries.md`'s new row and
dependency-direction bullet, and `testing-harness.md`'s new section all
cross-link to `shell/controls.md` and back reciprocally; no orphaned or
one-directional link found. Placing the Controls contract page under the
`shell/` doc path and "Shell" nav bucket is consistent with existing
precedent (`shell/layout-profiles.md`, `shell/applet-runtime.md` document
`src/profiles`/`src/applet_runtime`, not `src/shell` itself), so it is not a
boundary-direction inconsistency. I found no consumer of `QindaQt.Controls`
outside `src/controls` yet (grep-verified), and the S3 AppShell outcome
(`1787865666`) states AppShell "depend[s] only on public `QindaQt.Controls
1.0`" — consistent with the current one-directional dependency the module
boundaries page describes; no future-shell expectation contradicts it today.

## Requested Cora action

All four are documentation/API-contract findings, not build/behavior defects,
and none block the currently-paused compiler lane. Suggested next step: when
you next touch `docs/wiki/shell/controls.md`, add the ThemeCard `available`
row text, one sentence warning against overriding `enabled` directly on
Button/ThemeCard, one sentence noting DegradedNotice's inherited StateCard
surface and the `reason`→`message`/`retryText`→`actionText` relationship, and
one sentence on FormRow superseding the wrapped editor's own accessible
naming. None require a QML source change or a new module revision — they are
namable, stable contracts today, just under-documented.

I remain available for read-only follow-ups under the adaptive partnership.
Marking my record `idle` pending your next request; no acceptance of the
candidate is expressed or implied.

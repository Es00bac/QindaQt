# Juno Park: Q1–Q4 design questions reconciled against the integrated tree

- Timestamp: 2026-08-28T04:58:40Z
- From: Juno Park, native-application design lane
- Closes the follow-ups to my four open-question records against the current
  integrated reality (QST-1 landed; Controls S2 is an under-review candidate;
  Text Editor S1 is Linnea's uncommitted source candidate). This appends to
  the thread; the question records and earlier replies are not edited.
- Detailed editor findings:
  `../first-party-native-apps/1787892960-juno-park-experience-review-handoff.md`

## Q1 — shell customization (1787853801)

Resolved as designed. The editor slice consumes no `shell_customization`
surface and claims no live preview; it is an ordinary Wayland top-level per
the manager's answer (`1787856823`, Q1.3). The canvas-first S5 rule from my
handoff §S5 remains the standing plan; no live-subscription work is implied by
anything that landed.

## Q2 — platform services (1787853802)

- Q2.1 (`ServiceAvailability` values module): still future; nothing landed in
  `src/sdk/` yet. No first-party consumer pretends otherwise. Unchanged.
- Q2.2 (fonts): the editor takes `fontFamily`/`monoFontFamily` from theme
  schema v1 and leaves base point size at the QST default
  (`src/apps/text_editor/ui/editor_appearance.cpp:45-48`); Settings1-backed
  `fonts.*` consumption is declared later application-composition work
  (`docs/wiki/apps/text-editor.md:110-112`). Consistent with Samira's
  Q2.2 split; tracked, not owed by S1.
- Q2.3 (degraded presentation without service/UI coupling): honored.
  `DegradedNotice`/`StateCard` exist only in Cora's unintegrated Controls S2
  candidate and were correctly **not** consumed; the editor ships its own
  QST-typed banner. When Controls S2 integrates, the editor's banner stays
  valid (Widgets vs QML); severity-token adoption is requested as SF-J2 in
  the handoff, not as a Controls dependency.

## Q3 — Settings1 (1787853803)

Honored in full. The editor has zero Settings1/transport includes; Ada's
option-(a) allowance (app-owned domain composition over the public client) is
exactly what a future editor preferences slice would do. Q3.3 (no grouped
atomic Apply with the current client) remains respected app-wide.

## Q4 — themes/profiles (1787853804)

- Q4.1: closed as answered. Theme schema v1 preserved (`ThemeSpec` has no
  point size; `DesignTokenDeriver::derive` rejects `schemaVersion != 1`);
  accessibility inputs are caller-owned
  (`accessibility_inputs.h:6-8`). One residual caller-policy choice is now
  explicit: the editor derives with default inputs and therefore never arms
  the `highContrast` input, including for `qinda-high-contrast` (NF-J7 in the
  handoff) — a decision to document, not a defect.
- Q4.2 (derived user-profile persistence ownership): **still open**, owner
  still unnamed. Nothing in the editor slice touches user-profile persistence,
  so it stays safely deferred.

## Lane status

Nothing in this reconciliation changes path ownership or authorizes edits. My
review deliverable to Linnea is complete; boundary/design questions relevant
to the next first-party slices (second-app reuse ledger) are covered by
Rowan's D-1–D-6 ledger and ADR-0022's revisit clause.

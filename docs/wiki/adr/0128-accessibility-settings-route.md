# ADR-0128: Add the Settings Accessibility route

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** First-party Settings
- **Supersedes:** None
- **Superseded by:** None

## Context

Settings1 schema v2 has carried `accessibility.highContrast`,
`accessibility.reducedMotion`, `accessibility.reducedTransparency`, and
`accessibility.textScale` since the schema landed, and the Qt platform theme,
the shell runtime, and the QST-1 token deriver already consume them. Until now
the only way to change them was to edit the user-overrides file by hand. The
schema also defines `accessibility.screenReader`, which nothing consumes.

[ADR-0048](0048-settings-center-navigation-and-route-ownership.md) fixes how a
route joins the Settings Center: a closed component enum, a registry entry, and
one presentation branch, with the route owning its model, scope, and page.

## Decision

1. **One route, four keys.** `accessibility` is registered as the eleventh
   built-in route, appended last so existing indices and `Ctrl+digit`
   shortcuts stay stable. Its `SettingsClient` is scoped to exactly the four
   consumed keys on its own transport, constructed by the Settings executable
   and passed to QML as a route model, following the Appearance composition.
2. **The reserved key stays out.** `accessibility.screenReader` is not scoped,
   read, or written until a consumer exists; the route's boundary scan rejects
   any source that names it. Exposing a switch that changes nothing would be a
   false affordance.
3. **Draft, then per-key Apply.** The page edits a draft and commits on Apply.
   The public client writes one key per transaction, so Apply sequences the
   changed keys in schema order from fresh snapshots, treats an equal-value
   conflict as applied, stops on any other conflict or rejection, and never
   replays an uncertain write — the same contract Appearance follows.
4. **Short labels, explanations in tooltips.** Visible text is limited to
   control labels, the live sample, status, and errors; each control's
   one-sentence explanation is its `Accessible.description` and attached
   `ToolTip`.

## Consequences

- Users can change the accessibility preferences the desktop already honors
  without editing files; the effect is visible in the live sample before Apply.
- The route ships in the shared `SettingsAppearanceRuntime` component like
  every other route, so the staged installed-package gates and release
  packaging need no new component.
- Route count assertions (registry, host loaders, construction and installed
  rows) move from ten to eleven; the new route has no keyboard shortcut.
- A future screen-reader consumer must add the key to the scope, a switch to
  the page, and remove the boundary guard in the same change.

## Revisit when

A consumer for `accessibility.screenReader` lands, a per-application override
model appears, or the public client gains multi-key transactions (which would
let Apply become atomic).

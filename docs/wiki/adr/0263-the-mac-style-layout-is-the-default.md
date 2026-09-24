# ADR-0263: The Mac-style layout is the default

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Shell (layout profiles), Settings service (schema defaults)
- **Amends:** The default selection in
  [ADR-0223](0223-one-stock-profile-per-distinct-feel.md) and the startup
  fallback in [ADR-0074](0074-compose-shell-preferences-through-settings1.md)
  and [ADR-0122](0122-adopt-saved-layout-preferences-live.md). Both stay in
  force otherwise.
- **Superseded by:** None

## Context

Every stock layout from ADR-0223 is available, but one of them has to be what
a new user sees and what the shell lands on when it cannot honor a saved
choice. That was the signature `qindaqt` layout, spelled as a literal in three
places: the Settings1 schema default, the distribution profile-defaults layer,
and the shell's startup and live-adoption fallbacks. The owner uses the
`macos-inspired` layout (menu bar and dock) unmodified and asked for it to be
the default (plan W17, 2026-09-24).

## Decision

1. **`macos-inspired` is the default `panels.layoutProfile`.** It is the
   default in `data/settings/schema-v2.json` and in the distribution layer
   `data/settings/profile-defaults/qindaqt.json` (the file name is the
   distribution profile, not the layout).
2. **The shell falls back to the same layout.** One constant,
   `DefaultLayoutProfileId` in `src/shell/runtime/default_layout_profile.h`,
   serves the no-service startup, the deleted or renamed saved selection at
   startup, and the live-adoption case where neither the saved nor the running
   layout survives a catalog reload.
3. **The theme default does not change.** `appearance.theme` stays
   `qinda-dark` in the distribution layer. Only when Settings1 is unavailable
   does the shell use the selected profile's own `defaultTheme`, as before;
   that is now `qinda-macos`.
4. The legacy `schema-v1.json` keeps its historical `qindaqt` default; it only
   describes stores written before schema 2.

## Consequences

- A new user, and any session whose saved layout is gone, gets the menu bar
  and dock. Users who already saved a layout keep it.
- `tst_shellpreferencevalues` fails if the shell constant, the schema default,
  the distribution default, or the installed stock profile disagree.
- The `qindaqt` signature layout remains a stock choice in Settings.
- The developer preview tool (`qindaqt-shell-preview`) still defaults to
  `qindaqt`; it is not a user-facing default.

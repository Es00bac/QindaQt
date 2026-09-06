# ADR-0080: Resolve first-party appearance from confirmed Settings1 preferences

- Status: Accepted
- Date: 2026-09-05

## Context

The shell and Settings preview observed `appearance.theme`, while Text Editor,
Terminal, and File Manager loaded a command-line default once. The visible
Light, Dark, and System preference did not consistently affect a valid selected
theme, so applications could disagree after Apply or restart.

## Decision

A public `AppAppearance` module owns one pure resolution policy and one
GUI-thread controller over a borrowed, scoped Settings client. Consumers scope
`appearance.theme` and `appearance.colorScheme`. An explicit `--theme` locks
the process theme; otherwise a confirmed snapshot updates the application live.
Invalid, partial, unavailable, or uninstalled preferences retain the last
validated theme.

Light retains an installed light theme and Dark retains an installed dark or
dusk theme. System follows live platform scheme changes; an unknown platform scheme falls
back deterministically to dark. An incompatible or missing preference resolves
to `qinda-light`/`qinda-dark`, then another compatible installed theme. An
explicit high-contrast theme remains selected across scheme changes. Settings
preview and shell consume the same pure resolver. Choosing a theme card updates
the user draft's scheme to match light or dark/dusk; persistence remains the
existing Settings route transaction.

The controller publishes validated `ThemeSpec` changes. Widget applications
adapt that value through their native appearance adapters; QML applications
publish it through their engine-owned `TokenFacade`. It owns no persistence,
transport, QML engine, widget, or platform palette.

## Consequences

All first-party surfaces converge on confirmed appearance without reading the
settings file or duplicating theme fallback rules. Existing explicit CLI and
terminal profile choices remain available. Adding another scheme or theme
variant requires extending the resolver and its compatibility tests.

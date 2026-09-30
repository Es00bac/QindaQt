# ADR-0303: Resolve window application identity through the neutral catalog

- Status: Accepted
- Date: 2026-09-29
- Owners: Application catalog and compositor integration
- Scope: Cached XDG application identity metadata used by compositor presentation

## Context

Wayland windows may report an application ID or resource class that differs
from the installed desktop-entry ID. Resolving only that exact ID can leave
rolled-up chips with no application-specific icon, and KWin's inherited window
icon may be the generic Wayland mark. The pure desktop-entry parser already
validates StartupWMClass, but the neutral ApplicationCatalog previously
discarded it. Compositor code must not depend on shell presentation or private
KDE service internals to match window identity.

## Decision

Retain validated StartupWMClass on the public application-catalog entry.
WorkspacesApps takes one bounded XDG catalog scan when constructed and exposes
a window lookup that checks exact desktop-entry and application IDs first.
Only when those fail may a case-insensitive StartupWMClass match resolve, and
it must identify exactly one catalog entry; ambiguous aliases return no
metadata. Existing exact find and launch behavior is unchanged.

The compositor consumes this WorkspacesApps boundary and never falls back to
KWin's inherited Window::icon() when no app-specific icon is available. The
chip displays a monogram derived from the resolved application label, with the
full label and caption available on hover.

## Consequences

- Window presentation obtains stable app names and icon theme candidates
  without reparsing desktop files for every chip or depending on ShellIcons. The
  pure static catalog targets are position-independent because WorkspacesApps
  composes them into the loadable compositor module.
- Desktop-entry IDs remain authoritative when an alias points elsewhere;
  conflicting aliases fail closed rather than selecting an arbitrary browser
  or application variant.
- A WorkspacesApps instance sees catalog metadata from its construction-time
  scan. A new instance/session observes later desktop-entry installation or
  removal.
- Scanner projection, exact-ID precedence, case-folded aliases, ambiguous
  aliases, generic icon rejection, and monogram painting have focused tests.

## Revisit when

A shared live catalog refresh is introduced, the parser changes StartupWMClass
validation, or another consumer requires a different deterministic alias
policy.

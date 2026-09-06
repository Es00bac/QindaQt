# ADR-0081: Project confirmed appearance into native and grouped chrome

- Status: Accepted
- Date: 2026-09-05

## Context

First-party applications and shell surfaces follow confirmed appearance
preferences, but QindaDecoration painted a fixed light title bar and grouped
chrome constructed its fixed default palette. Dark and high-contrast sessions
therefore retained light window chrome.

## Decision

The compositor owns one scoped Settings client and one public AppAppearance
controller for `appearance.theme` and `appearance.colorScheme`. Its validated
ThemeSpec is adapted to a Hybrid ChromePalette once per change. Grouped chrome
rebuilds its immutable plans from that palette. Native Qinda decorations receive
the same colors through a process-local QObject property and repaint; decoration
recreation receives the current value. This property is an in-process handoff,
not a persistent or cross-process protocol.

QindaDecoration uses KDecoration's per-window palette when the Qinda property is
absent, so the plugin remains usable under a foreign compositor configuration.
Invalid or unavailable preferences retain the AppAppearance controller's last
validated palette. Geometry, topology, buttons, and window action policy do not
depend on appearance.

## Consequences

Native and grouped title chrome follows Light, Dark, System, and explicit
high-contrast choices live and for newly decorated windows. The compositor adds
no Settings service and writes no host configuration. New chrome color roles
must be added to the ThemeSpec-to-ChromePalette adapter and its contrast tests.

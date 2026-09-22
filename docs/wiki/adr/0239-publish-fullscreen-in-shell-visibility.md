# ADR-0239: Publish fullscreen in the shell visibility snapshot

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** compositor (shell visibility), shell runtime
- **Supersedes:** None
- **Superseded by:** None

## Context

The dock must disappear over fullscreen applications even if a stale pointer
reveal or popup hold remains. The visibility snapshot initially supplied frame
geometry and `maximized` but no fullscreen state. Geometry alone cannot
distinguish a fullscreen window from an ordinary maximized window in an
overlay-only layout: both may fill the output. KWin also tracks maximize and
fullscreen independently, so excluding all maximized windows from the
fullscreen rule leaves some fullscreen clients revealable.

## Decision

The compositor publishes KWin's `isFullScreen()` state in each admitted window
of its atomic `ShellVisibilitySnapshot`. The publisher refreshes on
`fullScreenChanged`. The shell hides non-`never` panels when an active
fullscreen window covers their output, even if it is also maximized. For a
publisher that omits the new field, the shell uses output coverage only when
the window is not maximized. This preserves intentional reveal of ordinary
maximized windows in overlay-only layouts.

`fullscreen` is an optional boolean field in the existing schema-1 payload.
An older shell ignores it; a newer shell defaults a missing field to false and
rejects a present non-boolean value. This permits the shell and KWin plugin to
be upgraded separately without making all panels safe-visible during the
transition. Complete handling of maximized fullscreen clients starts when the
updated compositor plugin runs in the next compositor session.

## Consequences

- The fullscreen and maximized facts are sampled in one coherent compositor
  generation; the shell does not join independent task facts to visibility.
- An updated shell against an older plugin still handles non-maximized
  fullscreen clients, but cannot prove that a maximized output-filling client
  is fullscreen. That compatibility limit ends after the compositor starts
  with the updated plugin.
- A foreground transition into fullscreen clears pointer reveal leases on
  every output intersected by the old or new active frame before policy
  evaluation, including a spanning window's secondary output.
- Policy, decoder, and wire roundtrip tests cover maximized plus fullscreen,
  malformed and omitted fields, and the publisher's additive field.

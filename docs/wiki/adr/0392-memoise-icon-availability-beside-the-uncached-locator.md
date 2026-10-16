# ADR-0392: Memoise icon availability beside the uncached locator

- Status: Accepted
- Date: 2026-10-16

## Context

The task list asks whether an icon resolves in the theme chain for every row on
every publication, to choose between the real icon and a typed fallback glyph.
`IconThemeLocator` deliberately remembers no result: a file created, removed or
swapped for an escaping symlink must be seen by the very next lookup, because
the image provider's confinement depends on that
([Shell iconography](../shell/iconography.md)). Profiling a 16-window strip
under a title spinner put about 87 percent of the CPU in `statx` from this
probing.

## Decision

Add `IconAvailabilityCache` in front of `IconThemeLocator::hasIcon` **only**. It
answers a yes or no used for a presentation hint, never a path, and the image
provider never consults it, so the pixels a surface loads are still resolved and
confined fresh. Verdicts are bounded and expiring: positive for 30 s, then one
metadata check of the remembered file keeps or drops it; negative for 5 s,
short because a Wine program's extracted icon is written after its window
appears ([ADR-0230](0230-name-and-picture-the-game-behind-a-launcher-class.md));
at most 512 entries, keyed by the locator's own clamped request tuple, with a
name the locator refuses answered without being retained. The clock is
injectable. The shell replaces the object when the icon theme or roots change
(`setIconThemes`), which invalidates everything.

## Consequences

`IconThemeLocator`, its freshness test and its confinement contract are
unchanged. A removed or replaced icon can be reported as present for at most
the positive TTL, and only for the hint; a newly installed icon is noticed within
the negative TTL. Tests: `qindaqt.shell-icons-availability-cache` (hits,
revalidation, negative expiry, invalidation, bounds, hostile names, injected
clock) and the task-list update-cost benchmark.

## Revisit when

A second hint consumer appears with different freshness needs, or inotify based
invalidation becomes cheaper than the TTL.

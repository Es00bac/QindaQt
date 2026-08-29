# Rhea Calder — virtual desktop current-base material seam and midpoint

- Timestamp: 2026-08-28T11:44:49Z
- State: **working**
- Public first parent: `0a547df33d9a31b969d78b4ca649d0b39dc04797`
- Reviewed second parent: `f28f443b7aae2d635481f49e847a7e1e1a3b573b`
- Preserved reviewed source parent: `fd9faab5ab79017be903dafc6f0587d09c511f49`

The staged non-destructive merge contains exactly 23 paths: all 20 reviewed
virtual-desktop paths plus only the KWin control endpoint and its compositor
architecture/reference pages. Its sorted path-manifest SHA-256 is
`6d680f330e3bfca5135ce3a2d28eadd5d930163d70a3e7a747541f8270268eb6`.
There are no deletions, missing reviewed paths, or edits to Notification Live,
Display D0/D1, Power, Controls, or Text Editor owned implementation paths.

## Material current-base seam

Public main now integrates Notification Live and the development-only
`DevelopmentShellSurfaces` method. Its accepted filter intentionally admits
only `notification-popup` and `notification-center`, while Dorian's reviewed
whole-desktop topology requires compositor-owned mapped/committed evidence for
the production `dock`. The candidate makes the minimum additive KWin change:
keep the production gate and unrelated-surface rejection intact, add exactly
`dock` as the third allowlisted qualification scope, and record ADR-0026 as the
narrow successor to ADR-0020's original two-role allowlist. Notification Live
continues selecting only its exact two roles.

## Safe evidence so far

- Focused Python units: 37/37 pass.
- Python compilation: pass.
- Source shape: 993 files, zero skipped/issues; endpoint remains 487 nonblank.
- Documentation/navigation: 64 documents, pass.
- Exact compositor D-Bus descriptor static check: pass.
- Staged whitespace, 20-to-23 path set, reviewed-parent retention, and owned-
  contract diff checks: pass.

One exploratory no-argument invocation of the descriptor checker exited 2 with
its usage message before the exact two-argument command passed; no product or
runtime state changed. Display D2 still owns the compiler/private-runtime lane.
Rhea has not compiled or launched bubblewrap, a compositor, a bus, a session,
the boot row, any UI, or any host endpoint. Next is the final staged-tree audit,
merge commit, exact-tree safe replay, and a durable request for lane release.

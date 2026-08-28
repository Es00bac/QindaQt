# ADR-0049: Capture the private parent framebuffer for S2 qualification

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Session and display qualification
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0026](0026-contain-virtual-desktop-qualification.md) defines S1 as a
QPainter KWin virtual-backend boot. KWin 6.6.5's ScreenShot2 plug-in requires
an EGL backend, so that row cannot safely grow a screenshot assertion without
changing its qualified backend. The next outcome must show the integrated
desktop, inject one real shell shortcut without contacting the host seat, and
retain S1 as an independent regression boundary.

## Decision

S2 is an additive row. Inside the existing empty-root bubblewrap boundary it
starts Weston 15 with the headless backend, pixman renderer, kiosk shell,
`1920x1080@1`, a fake private seat, no configuration, and the exact private
socket `qindaqt-parent-wayland`. KWin starts with `--windowed`, connects only to
that parent, and publishes the separately named `qindaqt-<12 lowercase hex>`
child socket used by every QindaQt process. Evidence validation rejects a child
socket equal to the parent rather than trusting construction alone.

Weston's debug extension is enabled only in this socket- and namespace-private
parent. `weston-screenshooter` receives a complete allow-listed environment
whose sole display endpoint is that parent socket. The fresh capture is renamed
`desktop-1080p.png`, parsed without an image-library fallback, and accepted only
when it is a checksum-valid, exact 1920x1080, bounded RGB/RGBA PNG with at least
16 colors across the complete deterministic sample grid. The compositor-observed
notification-center rectangle is sampled independently and must contain at
least 16 colors; its exact geometry and full-region digest bind captured pixels
to the interacted surface. The artifact, frame/region digests, byte count,
dimensions, and both color-diversity values are archived before the private run
root is removed.

The interaction probe first proves that no mapped, committed, active
notification-center surface exists. It sends exactly Meta press, N press, N
release, and Meta release to scenario-gated `Compositor1.InjectTestInput`. It
requires the stable development-device identity and then polls the compositor-owned shell-surface
inventory until exactly one notification-center record is mapped and committed
on KWin's one `WL-<zero-based decimal index>` output by the authenticated shell
PID. It never opens uinput or a host input node.

KWin also observes Weston's fake seat as one anonymous pointer and one
anonymous keyboard device. S2 requires that exact zero-vendor/product pair in
addition to the one combined QindaQt development device. These are private
parent-protocol devices inside the empty-root sandbox, not host-seat evidence.

S2 preserves S1's service/application/topology, production-role PSS ceiling of
1,048,576 KiB across compositor, session, notification host, shell, Settings1,
Audio1, Settings, and Text Editor, PID/start-time authenticated teardown,
post-cleanup survivor observation, durable failure archive,
manager allocation token, and cross-worktree private-runtime lock. Weston is a
qualification parent and teardown role, not a QindaQt production role, so its
memory is excluded from the product aggregate but its exact identity and exit
remain mandatory.

## Consequences

- S1 remains the render-independent boot regression and makes no screenshot or
  input-action claim.
- S2 proves one private 1080p rendered frame and one private-seat shell action;
  it does not qualify OpenGL, a DRM render node, physical input, multiple
  outputs, fractional scale, WUXGA/1440p, themes, or screenshot baselines.
- Weston and weston-screenshooter are discovered build tools. The S2 row is
  registered only when both are present and remains inaccessible without the
  same explicit private-runtime allocation as S1.
- The debug/screenshooter protocol is test infrastructure, not a product API
  and not permission to expose a capture protocol on a user session.

## Revisit when

Revisit when KWin's non-EGL backend gains an authenticated capture path, when a
production screenshot portal is in scope, or when CI replaces Weston with an
equivalent private parent. Any replacement must preserve distinct sockets,
private-seat-only input, exact frame validation, durable evidence, and bounded
teardown.

# ADR-0119: Authenticated window-preview channel for hover thumbnails

- **Status:** Accepted (shell side implemented; compositor renderer is the
  remaining bounded piece, see Consequences)
- **Date:** 2026-09-10
- **Owners:** Shell / task list, Compositor / KWin integration
- **Supersedes:** None
- **Superseded by:** None

## Context

Hovering a dock tile should show a thumbnail of the window (for a container:
its primary active-page member). No window-image path exists: the exported
KWin 6.6 development headers expose no supported window-texture readback to
plugins, and the shell is a separate Wayland client that cannot sample
compositor textures. Meanwhile the dock's hover UX contract (tooltips as
popup windows, `windows.read` gating, displayed-revision fencing) already
exists in the task-list applet slice.

## Decision

1. **Channel.** One new authenticated method on the compositor's
   `CompositorShell1` surface (the ADR-0061/0073 credential family):
   `WindowPreview(windowId, maxWidth, maxHeight) → (ok, width, height,
   stride, bytes)`. The renderer must capture the window's current texture
   aspect-fit inside the bounds, in a bounded ARGB32 buffer, using the same
   task-facts sampling rules. Gates: the caller must be the authenticated
   shell panel owner; the `windowId` must be a task-listed window in the
   current generation; non-normal and bound-shell windows are rejected; a
   per-caller rate limit bounds captures.
2. **Shell seam.** The applet controller consumes a null-able
   `TaskListAppletPreviewPort`; the production port is a producer-side D-Bus
   client bound to the exact compositor owner. Results are matched against
   the requested `(windowId, revision)`; at most one capture is in flight
   and a newer request supersedes an older one. Captured images reach QML
   through a token-addressed image provider (`qindaqt-task-preview`);
   tokens are monotonic and never reused, and the cache is bounded.
3. **Presentation.** The dock strip hosts one preview card per strip,
   positioned above the hovered tile, refreshed at most every 500 ms during
   a static hover, closed on leave/activation, and suppressed in favor of
   the existing text tooltip whenever the port is absent or a capture fails.
   `windows.read` denial and non-ready phases disable previews entirely.

## Consequences

- The entire shell-side pipeline (seam, controller arbitration, image
  provider, hover card, tooltip fallback) is implemented and unit-tested;
  with no port wired, presentation keeps today's tooltip behavior, so
  shipping the shell side alone changes nothing user-visible.
- The compositor endpoint and its renderer are **not yet implemented**: the
  exported KWin 6.6 headers offer no supported texture-readback API, and the
  renderer needs the nested-KWin harness to qualify. The bounded remaining
  work is (a) the endpoint with the gates above over a renderer seam, and
  (b) a renderer behind that seam using KWin scene internals
  (`OffscreenEffect`-style redirection plus GL readback, or a
  plugin-granted ScreenShot2 path). Until it lands, the production
  composition keeps the port unwired by design.
- Hostile-payload validation (stride/byte bounds, dimension caps) belongs to
  the client and is required before the port can be wired.

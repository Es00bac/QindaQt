# ADR-0383: Coalesce compositor change signals and repaint titles in the title band

- **Status:** Accepted
- **Date:** 2026-10-16
- **Owners:** Compositor plugin (window registry, Hybrid chrome, shell task facts)
- **Supersedes:** None
- **Superseded by:** None

## Context

On 2026-10-16 the owner's idle desktop kept the compositor main thread at
33–42 % of a core and the shell at 29–44 %. Terminal agents (Claude Code,
Codex) animate a spinner in their window title about ten times a second, and
every caption change ran the whole window pipeline:

- `ManagedWindowRegistry` folded `captionChanged` into the same zero-delay
  `windowsChanged` as frame and state changes, so `Compositor1.WindowsChanged`
  was broadcast 9–14 times a second to every session-bus peer, although nothing
  in production subscribes to it (findings item 6; it also fed the session bus
  queue growth of item 1).
- The Hybrid session answered every `windowsChanged` with a full
  `synchronizeChrome()`: every container was re-planned, and every overlay
  re-rendered and re-uploaded a full-frame chrome image, about 8 MB per tick for
  a maximized terminal container, even when its plan had not changed at all.
- Task facts were sampled on the change and sampled again when the shell read
  `TaskListSnapshot` in response, the store encoded JSON twice per sample, the
  endpoint decoded its own reply to learn whether the caller was authorized,
  and both the change hint and the read made a blocking
  `GetConnectionCredentials` call on the compositor thread.

KDE's libtaskmanager answers a title change with a per-row `dataChanged` and
GNOME Shell's window tracker with a property notification; neither rebuilds
its window model. This decision gives QindaQt the equivalent without taking a
dependency on either.

## Decision

1. **Captions have their own rate-limited path.** The registry delivers caption
   changes through `windowCaptionsChanged(windowIds)`, coalesced per window to
   at most one delivery per 250 ms with a trailing flush, so the final title of
   a burst always arrives. `windowsChanged()` no longer carries captions.
2. **A title-only change never re-plans a container.** The session re-derives
   only title text (`HybridChromePlanBuilder::refreshTitles`, the same page and
   member title rules a full build uses) and publishes it with
   `KWinChromeManager::updateTitles`, which refuses anything but a text change.
   A full `synchronizeChrome()` runs only when a title would move geometry (an
   unnamed rolled-up strip, whose width is measured for its label).
3. **Chrome overlays compare plans and repaint the smallest region.** Each
   overlay rasterizes into two scene images: a *title band* (the shared title
   row plus a 4 px spill margin) and a *body* whose band rows are transparent.
   An identical plan repaints and uploads nothing; title text, tab geometry
   (split-deck glide frames) and hover state repaint the band alone; anything
   else repaints both. Member titles are carried in the plan but painted by
   KDecoration, so they repaint nothing. A style change that may swap only the
   button painter, which plans cannot compare, forces one full repaint.
4. **Public change signals are rate-limited, not removed.**
   `Compositor1.WindowsChanged` and the targeted
   `CompositorShell1.TaskListSnapshotChanged` reach the bus at most once per
   250 ms, with a trailing flush. A change after a quiet period is still
   announced at the end of the same event-loop turn.
5. **Task facts are served from the cache.** `TaskListSnapshot` returns the
   already-encoded generation; only the rate-limited refresh samples KWin. The
   store compares candidates by value, so an unchanged sample encodes nothing
   and a changed one is encoded once. Controllers return their authorization
   decision with the payload instead of having the endpoint decode it.
6. **Caller credentials are cached per unique bus name.** The first request
   from a connection makes one `GetConnectionCredentials` call with a 500 ms
   timeout (`QDBus::Block`, never a nested event loop); the answer is cached
   until `NameOwnerChanged` reports that owner gone. Unique names are never
   reused, so a cached PID cannot be borrowed. Failures are not cached.

## Consequences

- The deterministic budget row `compositor.title-change-budget` drives one
  title at 10 Hz for 10 simulated seconds through the production components:
  100 changes produce 41 caption deliveries, 41 `WindowsChanged`, 41 task-fact
  samples and 41 `TaskListSnapshotChanged`, no container re-plan, no chrome
  publication, no full-frame render, 41 title-band renders (13.9 MB uploaded
  instead of 1.48 GB) and one credential lookup. The same row's model of the
  previous wiring reports 100 / 100 / 200 / 100, 100 re-plans and 100 full
  renders.
- Consumers that relied on `WindowsChanged` arriving once per change now see
  at most four per second; every signal remains a "re-read the snapshot" hint,
  which was already the documented contract.
- During sustained change the dock's facts and their action generation can lag
  by up to 250 ms; an action in that window is answered `stale` and the shell
  rereads, exactly as for any race with the generation fence (ADR-0061).
- Band and body stacked together are identical whichever repaint produced
  them, and match one whole-frame `ChromeRenderer::paint()` within one
  antialiasing step on the wide focus glow (QPainter strokes it slightly
  differently on a shorter device). `compositor.chrome-overlay-raster` pins
  both properties across scales 1, 1.25, 1.5 and 2, translucent materials,
  split decks, rolled strips and hover.
- `compareChromePlans()` must learn every new `ChromeRenderPlan` field; its
  size assertions fail the build when the struct changes.
- Tests: `compositor.change-rate-limiter`, `compositor.chrome-overlay-raster`,
  `compositor.shell-credential-cache`, `compositor.title-change-budget`, and new
  rows in `compositor.hybrid-chrome-plan-builder` and
  `compositor.shell-task-facts`.

## Revisit when

A production client needs per-change `WindowsChanged` delivery (give it a
typed, rate-limited per-window signal instead of lifting the limit), a title
row can wrap or grow taller than the band, or KWin gains partial-texture
updates for `ImageItem`, which would let one image carry both regions.

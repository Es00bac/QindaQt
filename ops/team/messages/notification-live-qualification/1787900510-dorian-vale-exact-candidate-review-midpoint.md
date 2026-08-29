# Dorian Vale — exact Notification Live review midpoint

- **Timestamp:** 2026-08-28T07:01:50Z
- **Owner:** Dorian Vale, independent KWin/API and nested-session evidence reviewer
- **Exact candidate:** `557260a50faaf083733afe5972ad6541ef398108`
- **Tree / parent:** `8f9f131461157b33bb88e0b4a46811e2308c9329` /
  `c4982697858c083828bd406f1aa56c4e942bcc10`
- **Current finding:** no blocking P0/P1/P2/P3 finding yet; verdict remains open

## Immutable identity and scope

The detached review worktree is clean at the exact candidate. I independently
recomputed 74 changed paths, `+5825/-224`, and path-manifest SHA-256
`3be3d516f941c62d0d8f227258d0669fe71e336d787af9e7da3435755a98e731`.
`git diff --check` passes.

## Independent evidence already complete

- Fresh exact Debug configure and requested product/test build completed all
  479 actions successfully in `build/dorian-notification-review-debug`.
- Fresh focused gate passed 11/11: D-Bus descriptor, development-input codec and
  injector, runtime options, Notification Live Python unit/syntax, session
  supervisor, quieting bridge, and three notification QML rows.
- `tools/validate-docs` validated 44 documents; `tools/check-source-shape`
  checked 800 sources; strict MkDocs build passed.
- The staged Release KWin plugin passes `ldd -r`, depends on exported
  `LayerSurfaceV1Interface`/`WaylandServer::findWindow` ABI, and contains no
  `LayerShellV1Window` reference.
- Soren's exact post-extraction 1080p CTest command/result is preserved and
  passes in 9.86 seconds.

## Live/next gate

Soren's earlier five-row and race-10x totals are present only as durable board
summaries after the final smoke overwrote `LastTest.log`. I will not qualify
those rows from summaries. I am therefore replaying the staged installed five
rows and race-10x from the clean exact Debug build under the harness's fresh
temporary HOME/XDG trees, private `dbus-run-session`, and new-session process
groups. No host display/input/session bus/config/hardware endpoint participates.


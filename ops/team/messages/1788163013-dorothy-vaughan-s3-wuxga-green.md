# S3 exact WUXGA row is green with contained teardown

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T01:56:53-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: WUXGA green; later private rows not started

The exact registered package fixture and
`desktop.virtual.interactive.matrix.single-wuxga` pass 2/2 in 6.75 seconds
after closing the private Qt 6.11.1 declarative/SVG runtime closure.

Preserved result:
`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/d2e82f3d14d4787ce286caecd35ebdea`

- `result.json` SHA-256 `f0f3c27b…`, outcome success, return code 0.
- evidence SHA-256 `50f09adf…`.
- 1920×1200 screenshot SHA-256 `5b899096…`; the recorded content region is
  non-uniform and matches the interacted notification-center surface.
- topology is exact WUXGA, scale 1, qinda-light, xfce-inspired.
- Settings and Editor windows are mapped; dock and notification-center surfaces
  are mapped and committed.
- containment reports host display/input/session-bus unreachable.
- bounded cleanup is true with `survivorPids: []`.

Full CTest log:
`/tmp/qindaqt-s3-selene-build/qt-private-wuxga.log`, SHA-256 `30801173…`.
A fresh host process audit finds no owned KWin, Weston, or staged QindaQt
survivor. Per the exact authorization, I stopped after WUXGA. The registered
1440p/125%, 1080p/150%, and dual-output rows remain unstarted; theme variants
need the queue's exact registered mapping confirmed before execution.

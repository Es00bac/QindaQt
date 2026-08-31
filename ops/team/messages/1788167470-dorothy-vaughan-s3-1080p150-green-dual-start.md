# S3 1080p/150% is green after bounded sampler repair; dual starts

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T03:11:10-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: 1080p/150% green; exact dual-output row starting

The manager-authorized owned-harness repair retains the sixteen-color floor and
scans the exact bounded region in deterministic row-major order until the floor
is reached or the region is exhausted. Focused 6/6, full contained-session
93/93, and `git diff --check` pass. The regression proves the former grid sees
one color in a genuinely nonuniform region; a separate large uniform region
still fails.

The exact single authorized rerun of
`desktop.virtual.interactive.matrix.single-1080p-150` plus its package fixture
passes 2/2 in 8.06 seconds. Preserved result:

`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/1b30bfe610d96520125119ffaefdaf8b`

- result JSON SHA-256 `dddf8f30…`, outcome success, return code 0.
- evidence SHA-256 `808c5f15…`.
- capture SHA-256 `c1bfffc2…`, exact 1920×1080; region digest remains the
  failure-run-identical `c4038887…` and satisfies the unchanged floor at 16.
- requested/render scale 1.5, 1280×720 logical topology.
- exact live `mate-inspired`/`qinda-dark` session and editor arguments.
- Settings and Editor plus both docks mapped; notification center active.
- aggregate PSS 155,001 KiB below the 1,048,576 KiB ceiling.
- host display/input/session bus unreachable; bounded cleanup and empty
  survivor list; fresh `/proc` executable inspection also finds zero.

Full CTest log:
`/tmp/qindaqt-s3-selene-build/qt-private-1080p-150-repaired.log`, SHA-256
`ba951482…`.

I am now starting exactly one registered `dual-1080p-horizontal` row. No private
runtime is alive at the transition.

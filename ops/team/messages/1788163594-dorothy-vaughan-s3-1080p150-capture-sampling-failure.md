# S3 1080p/150% stops at a sparse content-region sampling false negative

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T02:06:34-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: stopped at first failure; dual-output row not started

The package fixture passed, then exact registered
`desktop.virtual.interactive.matrix.single-1080p-150` failed with:

`desktop session qualification failed: captured content region is visually uniform`

Preserved result:

`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/64fc507bc5b63ec2d0cf9fb2a127034f`

- result JSON SHA-256 `cf857247…`, outcome failure, return code 1.
- sandbox-command SHA-256 `bc78d289…`; host display/input/session state remains
  absent by the recorded strict command contract.
- exact 1920×1080 PNG is 47,585 bytes, SHA-256 `98a48090…`.
- full deterministic framebuffer sample: 73 colors, so the frame is not blank.
- interacted logical notification-center geometry is `(824,46 440×640)`.
- exact 1.5-scaled physical region is `(1236,69 660×960)`.
- current fixed 64×64 region grid sees 15 colors; contract minimum is 16.
- raw region digest is
  `c40388875a4b939ed2959dab90871333879e9aa5b5317e1bbddf498ec48b4bc8`.
- exhaustive read-only inspection of the same decoded region finds 544 colors
  and reaches the sixteenth distinct color after 97,865 of 633,600 pixels.
- direct visual inspection confirms the qinda-dark notification center, title,
  sections, buttons, empty-state text, Settings, Editor, global bar, and docks
  are all visibly rendered.
- failure archival preserves the capture, sandbox command, interaction log,
  five probe logs, and every process log. The final probe shows mapped apps,
  both docks, exact 1280×720 logical output, and owned services.
- fresh `/proc` executable inspection finds zero owned survivors.

Full CTest log:
`/tmp/qindaqt-s3-selene-build/qt-private-1080p-150.log`, SHA-256 `295b66e1…`.

This is exact evidence that the sparse grid produces a one-color-below-floor
false negative for the dark/150% presentation, not evidence of a uniform
region. I made no edit and did not start dual-output. The smallest coherent
owned-harness repair is to keep the 16-color floor but scan the exact bounded
region until either sixteen distinct pixels are found or the region is
exhausted, recording the exact distinct count/digest as today. That strengthens
the evidence against sparse-sampling misses without relaxing the accepted
minimum. Please authorize that specific capture-validation/test change and one
serial 1080p/150% rerun, or route a different repair.

# S3 1440p/125% row is green; requested next 1080p/125% row is not registered

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T02:00:35-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: 1440p/125% green; stopped at the next under-specified requested row

The package fixture and exact registered
`desktop.virtual.interactive.matrix.single-1440p-125` row pass 2/2 in 12.80
seconds. Preserved result:

`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/dd22c6143ad53529a76639190ae4a6f5`

- result JSON SHA-256 `e8a60abc…`, outcome success, return code 0.
- evidence SHA-256 `831b4381…`.
- 2560×1440 screenshot SHA-256 `ed3d9123…`; 56 sampled colors.
- exact 1.25 requested/render scale and 2048×1152 logical topology.
- exact live `unity-inspired`/`qinda-dusk` session and editor arguments.
- Settings and Editor mapped; both docks mapped/committed; notification center
  active on `WL-0` after the authenticated four-event interaction.
- aggregate PSS 167,805 KiB below the 1,048,576 KiB ceiling.
- host display/input/session-bus unreachable; bounded cleanup and no survivors.
- fresh `/proc` executable inspection finds zero owned survivors.

Full CTest log:
`/tmp/qindaqt-s3-selene-build/qt-private-1440p-125.log`, SHA-256 `3db3b935…`.

I stopped before the next explicitly ordered 1920×1080-at-125% step because
there is no registered S3 interactive row for it. The closed S3 set in
`desktop_session_matrix.py`, the generated CTest registry, and the normative
testing-harness table agree on exactly four rows: WUXGA/light, 1440p-125/dusk,
1080p-150/dark, and dual-1080p/light. The source catalog does contain
`single-1080p-125.json`, but the S3 loader rejects it as unapproved; its older
virtual-output-only registration is absent from this focused build and, by its
own AGENT-CONTRACT, applies only count plus common mode/scale. It cannot prove
the requested apps, panels/docks, interaction, screenshot, resources, or
teardown.

Smallest no-code interpretation is to continue the remaining already-approved
S3 rows (`single-1080p-150`, then `dual-1080p-horizontal`), treating light/dusk/
dark as presentation assertions embedded in WUXGA/1440p/1080p-150 exactly as
the accepted harness specifies. If a distinct interactive 1080p/125% row and
separate theme repetitions are now required, that is a matrix-contract/source
expansion and needs explicit repair authority before any edit or runtime claim.

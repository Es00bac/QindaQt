# PF12/PF13 midpoint: schedule/service boundary underway

The manager approved the model-aligned `display.nightLight.*` keys and preserving the compositor's existing `org.qindaqt.KWin.NightLight` ABI. The Settings schema/profile defaults now describe the current `NightLightSettings` value bounds and tokens. I added stable output-ID opt-outs (using opaque IDs bounded to the public Display identity shape), a pure fixed-time/solar schedule calculator, and a resident Settings1-backed Schedule1 service skeleton with targeted frames tied to an initial nonce, the exact unique owner, cookie, and monotonic revision. The schedule client treats the signal receipt as authority and does not trust method replies.

The private qinda build is still running under the shared j24/l24 flock. Its first pass exposed a test-only Qt enum comparison formatting error; I corrected that assertion. The resident service does not yet have GeoClue acquisition or PF13 migration import/retry.

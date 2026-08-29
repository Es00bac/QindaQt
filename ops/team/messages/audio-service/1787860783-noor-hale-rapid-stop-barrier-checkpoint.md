# Audio1 repair checkpoint: rapid-stop barrier now bounded

The independent audit is closed at `1787860603-codex-audio1-exact-review-reject.md`: exact `6926aad9c93a757d06f32835db9962007ce2b195` is rejected with the five posted P2s and no further findings. I am continuing one cohesive repair commit on top of that candidate.

The fifth repair now has a concrete production-backend result in the private unreachable-runtime fixture. Component API loads are tracked by cancellables and drained before worker exit, and they are no longer started until `wp_core_connect()` accepts the private PipeWire core. The 250 immediate `start()`/`stop()` cycle regression, which previously leaked 501 descriptors and then exhausted `GWakeup` pipes, now completes in 0.56 seconds with its bounded `before + 5` descriptor assertion passing. The same test rejects stopped/superseded run generations and requires a fresh epoch on restart. No host audio graph or user bus was contacted.

Remaining gates: finish the resident-authority epoch/lineage audit and comments; run focused Debug/Release; ASan+UBSan including the 250-cycle production test; private isolated PipeWire/WirePlumber runtime and activation cleanup; broad registry; strict docs/link/source-shape/whitespace; then create a new non-amended repair commit and request exact-commit re-review.

— Noor Hale, Audio1 implementer

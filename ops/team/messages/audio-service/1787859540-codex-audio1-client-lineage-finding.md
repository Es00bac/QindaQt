# Audio1 exact review — client revision/epoch fail-closed finding

- Reviewer: `codex-audio1-exact-reviewer`
- Timestamp: `2026-08-27T13:39:00-06:00`
- Exact candidate: `6926aad9c93a757d06f32835db9962007ce2b195`
- Classification: **P2 — blocking public client authority/revision validation gap**

## Finding

The public client does not bind snapshot content or successful operation completion to its current exact `(owner, epoch, revision)` authority strongly enough:

1. `src/services/audio_client/src/audio_client.cpp:270-295` rejects a snapshot only when its revision is *lower* within the same epoch. A second valid snapshot with the same epoch and same revision but different devices/defaults/levels/capabilities is accepted and emitted. That lets model content change without a revision advance, contradicting the documented revision boundary.
2. `src/services/audio_client/src/audio_client.cpp:433-455` checks a result against the operation's initiating kind/epoch/revision, but does not compare a `Succeeded` result with the client's currently published snapshot. If the client fetches and publishes a new epoch while the mutation is pending, a delayed fabricated result claiming success entirely in the old epoch still passes `validateOperationResult()` and `exactInitiator`, then is emitted as `Succeeded` rather than `Uncertain`.

The second case matters without a service-name owner change: WirePlumber/PipeWire authority replacement advances epoch inside the same resident D-Bus owner. Audio1's architecture and reference require authority loss to make pending mutations uncertain and forbid replay/success claims across that boundary. D-Bus ordering makes the current implementation less likely to receive this from the current production service, but the exported client validator is expressly the fail-closed process boundary and must reject contradictory exact-owner replies rather than trust them.

The existing regression in `tests/services/audio_client/tst_audio_client.cpp:49-64` covers only a strictly lower revision. `tests/services/audio_client/tst_audio_client.cpp:111-130` covers a mismatched initiating revision, not a current-epoch replacement followed by an old-epoch success. There is no equal-revision/different-payload or current-lineage contradiction test.

Repair must treat `(same epoch, same revision, different payload)` as malformed while allowing an identical repeated fetch, and must turn any success that is incompatible with the client's current published epoch/revision into `Uncertain/malformed-result` followed by refetch. Tests should exercise both cases plus exact duplicate acceptance. The reviewer is continuing the remaining runtime/packaging audit; no candidate source was edited.

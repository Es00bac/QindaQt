# Audio1 exact review — blocking public-client completion finding

- Reviewer: `codex-audio1-exact-reviewer`
- Timestamp: `2026-08-27T13:31:26-06:00`
- Exact candidate: `6926aad9c93a757d06f32835db9962007ce2b195`
- Classification: **P2 — blocking public asynchronous API contract violation**

## Finding

`AudioClient` documents that all completion/error reporting is asynchronous, but local rejection and busy paths emit `operationCompleted` synchronously inside the public operation call, before that call returns its request ID.

Concrete evidence:

- `src/services/audio_client/include/qindaqt/services/audio_client/audio_client.h:23-27` is the public contract: the client is single-threaded, serialized, never replays uncertain mutations, and **all completion/error reporting is asynchronous through signals**.
- `src/services/audio_client/src/audio_client.cpp:318-345` allocates a request ID and directly emits `operationCompleted` for busy, unavailable, stale, invalid, incompatible, and unsupported requests. This call stack originates inside the public `setDefault`/`setVolume`/`setMute`/`moveStream` methods at lines 357-393, so the signal is delivered via the default direct same-thread connection before the public method returns.
- `tests/services/audio_client/tst_audio_client.cpp:132-154` concretely relies on the synchronous behavior by calling a public mutation and immediately reading `QSignalSpy::constLast()` without processing the event loop. There is no test enforcing the stated asynchronous boundary.

Impact: a consumer cannot correlate the completion using the returned request ID before its completion handler runs; same-thread handlers can re-enter client/UI state during the initiating call; and the behavior differs between locally rejected operations and transport-backed operations. This is an externally observable violation of the new exported public boundary, not merely an implementation preference.

The exact candidate must be repaired so every public completion, including Busy/Rejected/Unsupported local outcomes, is queued until after the initiating public call returns, with lifetime-safe cancellation/drop behavior if the client is stopped or destroyed before delivery. Tests must prove non-reentrancy and request-ID correlation rather than relying on the current direct emission. The candidate remains under review for additional findings; no source repair is being made in the reviewer worktree.

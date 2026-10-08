# ADR-0364: Configure Voice credentials through provider-owned secure storage

- **Status:** Proposed
- **Date:** 2026-10-08
- **Owners:** Voice public contract/Settings; Gabbee headless provider
- **Supersedes:** None
- **Superseded by:** None

## Context

Accepted ADR0233 keeps transcription, provider configuration and secret storage
with the headless provider; the desktop owns its graphical consumer. Voice1
currently describes selection and inventory but has no secure credential entry
or configured-versus-effective fallback projection. An unlocked matching
Secret Service item can become available after Gabbee's startup credential
cache was read: selecting ElevenLabs then still advertises only local Whisper.
A manager-observed graceful idle provider restart changed availability4 to5
without reading a secret or opening a microphone. Existing keys must be
reloadable graphically; asking users to enter the key again is not recovery.

## Decision

Add optional separately versioned org.qindaqt.VoiceConfiguration1 at
/org/qindaqt/VoiceConfiguration1 on the unique process that currently owns
org.qindaqt.Voice1. It owns no well-known name of its own. Do not add keys,
operations or capability bits to Voice1, or put credentials in Settings1.
Old providers remain usable; an unsupported extension disables credential
actions with a truthful explanation.

The public cohesive module owns bounded values/codec, injected transport,
owner/revision/singleflight client and Qt D-Bus transport. Settings owns one
small credential model and masked entry/save/reload presentation. Existing
Voice preference/transcription models remain their existing owners.

Snapshot uses a{sv}, schemaVersion1, independent monotonic revision, and:
configuredProvider/effectiveProvider bounded provider identifiers;
fallbackActive; credentialCached; credentialSource (none, secret-service,
environment, unresolved); environmentOverride; canConfigure; bounded closed
statusCode. These are configuration/runtime facts, never a key, key fragment,
hash, transcript, audio or upstream exception. Effective provider means the
engine actually selected/last used; an unknown engine reports unresolved
rather than restating configuredProvider. An active explicit process or
legacy environment override remains visible and takes precedence; saving a
wallet key must not claim that key became active while the override remains.

GetSnapshot is passive and reads only in-memory projection. It never reads or
unlocks a keyring, loads a model, activates the provider, starts dictation or
calls an API. The desktop discovers the existing Voice1 owner without
activation, sends extension calls directly to that unique name, and fences
every reply against the owner/token that issued it.

SaveElevenLabsKey(requestId, expectedRevision, key) and
ReloadCredentials(requestId, expectedRevision) are deliberate idle-only
operations. Key accepts at most512 UTF-8 bytes, nonempty printable ASCII without
whitespace/control characters; it is never echoed. Results use bounded closed
status/reason codes and revision/request identity. A stale revision conflicts,
capture/transcription/reload activity reports Busy, and a lost reply/owner
replacement/timeout reports Uncertain without replay. A successful result is
followed by fresh current-owner snapshot readback.

Reload reads the already saved credential through the provider's secure
storage port and refreshes the provider's transcriber/configuration explicitly.
It is the supported late-keyring-availability recovery and never requires
reentry. Read/store/reload failure preserves prior effective engine state and
reports an actionable sanitized state; it does not fall back to plaintext.
A successful persistence with failed effective reload is a distinct partial
outcome: saved key retained, reload unconfirmed, deliberate reload next.

The provider owns a bounded single-flight worker for potentially blocking
Secret Service I/O/reconfiguration, with copied requests/results and a GUI
thread completion hop; exported service classes expose no object-valued
internal signals. Cancellation/timeout cannot undo an already submitted
secret write: retain singleflight until the actual operation settles and
report uncertainty, never promise interruption or replay. Request/provider
shutdown drops late consumer results; an I/O call is not assumed interruptible.
Implementation must use the existing secure Secret Service owner and bounded
call facilities, not ConfigWindow or .env fallback, another secret store or a
general supervisor.

The entry is Password-masked, capped at512 characters, cleared immediately
when an operation is submitted/refused and on owner/page teardown. Key strings
exist only for the request/storage operation; QString/Python allocator copies
prevent a hard zeroization promise. No logs, diagnostics, persisted UI drafts,
Settings1 values or snapshot payloads may retain key material.

## Acceptance

- Pure codec bounds/type/unknown-field/refusal and existing Voice1 unknown-bit
  compatibility unchanged.
- Injected client current owner/revision, stale/lost reply, singleflight,
  uncertainty/no-replay, unsupported old provider and deliberate reload.
- Provider fake repository existing-key-after-startup, save failure/locked
  store, reload/transcriber failure, override precedence, idle Busy and
  owner/service teardown. Poison live microphone/API/real secret-store ports.
- Masked real Settings component entry/save/reload/clear, availability versus
  effective/fallback labels, missing extension and recoverable secure-store
  failure, no secret anywhere in result/log/Settings1.
- Separate focused strict/native and packaged consumer/installed recovery
  gates after root grants; no live key/API/microphone call from author fixtures.

## Consequences

Gabbee stays headless and owns its existing engines/configuration/storage.
Settings gains graphical secure entry and late-keyring recovery; the Voice1
wire remains unchanged. Optional extension support does not prove the cloud
account authenticated or any audio was transmitted. No implementation/native
qualification is established by this Proposed design.

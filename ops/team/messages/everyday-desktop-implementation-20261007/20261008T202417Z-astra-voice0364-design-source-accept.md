# Voice0364 bounded design SOURCE ACCEPT

Exact candidate: c54a676b288c7fa007d187d3ece7ffe0f1971596.
Read the complete Proposed ADR0364, new public value header and existing
QtVoiceTransport startup/owner/call behavior. No production implementation,
native test, credential read/write, microphone or API action was performed.

The optional same-owner extension, independent revision, deliberate Save and
Reload, existing-key recovery, no-readback values, environment precedence,
partial saved/reload-unconfirmed result and no replay preserve the intended
Voice1 compatibility and provider-owned storage boundary. This accepts the
design for implementation, not completed authentication or runtime behavior.

Implementation acceptance must still establish:

- The configuration transport discovers the existing owner without calling
  VoiceTransport.start()'s current StartServiceByName activation path.
- Same unique destination plus request/revision/generation fences are
  correlation and lifetime checks, not a newly proven wire-sender mechanism.
  Do not label QDBusMessage.service() as actual reply sender.
- The chosen existing Secret Service adapter has explicit bounded calls.
  A consumer timeout retains provider singleflight until actual I/O settles;
  a stale worker cannot replace an engine after a new capture has begun.
  Provider shutdown and worker completion use owned lifetimes, no raw late
  QObject callbacks. No secret/error-message/traceback logging.
- Saved-but-reload-failed keeps a truthful partial result; a configured
  environment override remains visible. Existing keys are reloaded without
  requiring user reentry.

No blocker in this proposed split/public values. The unresolved concrete
Secret Service adapter is explicitly a production implementation gate, not
permission to call an unbounded backend.

# Audio1 exact review: P2 backend result can bypass the wire contract

Reviewer: Codex Audio1 exact reviewer  
Candidate: `6926aad9c93a757d06f32835db9962007ce2b195`  
Decision impact: blocking P2

The service coordinator does not validate or normalize the complete backend
operation outcome before constructing a public `OperationResult`:

- `src/services/audio_service/include/qindaqt/services/audio_service/audio_backend.h:19-23`
  makes `reasonCode` an unconstrained public-backend value.
- `src/services/audio_service/src/audio_operation_coordinator.cpp:272-311`
  bounds/sanitizes only `diagnostic`; it copies `outcome.reasonCode` verbatim to
  the D-Bus result. An out-of-range `BackendOperationStatus` also falls through
  to `Failed` without a stable replacement reason.
- `src/services/audio_protocol/src/audio_validation.cpp:217-240` defines the
  actual public wire invariant: reason code must be at most 64 UTF-8 bytes,
  contain no NUL, and the complete result must validate.
- The coordinator's installed public contract at
  `audio_operation_coordinator.h:19-22` says the backend cannot bypass payload
  validation, but this path does exactly that.

Concrete consequence: a backend outcome whose reason is 65 ASCII bytes or
contains NUL is serialized by `AudioServiceObject::finishOperation()` as an
invalid public reply. A conforming AudioClient then has to downgrade the
service-produced reply to `Uncertain/malformed-result`; a direct D-Bus consumer
receives protocol-invalid data. This is not fail-closed at the authority
boundary and breaks the documented stable reason-code surface.

Required repair: validate/normalize every backend outcome in the coordinator
before publication (including enum status, reason code, diagnostic, and
lineage), emit a bounded stable fail-closed result for malformed backend data,
and add service tests for overlong/NUL reason, unsafe/overlong diagnostic, and
invalid status. Do not rely on the client to repair an authority's malformed
wire reply.

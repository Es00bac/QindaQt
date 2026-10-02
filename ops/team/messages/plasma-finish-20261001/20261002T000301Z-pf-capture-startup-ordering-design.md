# Authentic capture startup ordering — source design before edit

2026-10-02T00:03:01+00:00

Base consumer executable ca29910d15c99319834dd5a88c2485339fc1d015 plus own records d096e07d123129f103022971859119ecaabdc24f; fork68c4d74f903b7e8990dd5fd5d509ec8154eac1d1 stays independently reviewed and unchanged. Scope is one startup-publication ordering outcome. Historical response2 remains causally unresolved; the latest diagnostic row did not fail.

## Normative constraints and chosen boundary

Read ADR0318, proposed ADR0324, portal-capture reference, fork capture-authority/WIRE.md and installed-header source protocol.h. QCC1 Ready binds the actual post-exec broker PID/unique sender and CURRENT ownership of the capture well-known name. Consequently the name must exist before Ready reaches the compositor. Ready's bytes, role, generation, deadlines, fd3 and all helper fd4–7 meanings remain unchanged; helper Ready and capture consent ordering remain untouched.

Use two-phase startup of the existing capture-only backend, without adding a public grant or synthetic harness delay:

1. NativeCaptureAdmission exposes initialized-current-owner state separately from existing ready/admitted (unlocked pixels). Initialized means its actual monitor holds authenticated non-Unknown Unlocked, Locking, or Locked state for the retained peer/current native owner. The monitor already publishes such state only after nonce-targeted stateReceipt AND empty method reply, with generation/serial/owner admission. Existing ready() keeps its unlocked meaning for CaptureDialog; add a distinctly named initialized observation using the monitor's authenticated state transition and a current read-through. No guessed property, first signal, or own-harness receipt qualifies.
2. AuthorityCapture owns that observer and all existing resources, but has an explicit one-shot channel-start boundary after initialization. Before channel start, its lifetime timer must not call Channel::reconcile (which correctly rejects an unstarted identity). The callback cannot survive owner loss or restart the same closed authority. `available()` continues to describe setup; request-time `admitted()` still requires live channel and current unlocked native state.
3. Backend main creates RequestRegistry/AuthorityCapture/host/adaptors while unpublished. After a current initialized-state observation, register the object before claiming the fixed backend name, then start the authenticated Channel immediately in the same Qt thread. This preserves the compositor's required name-before-Ready order. Registration/start failure unregisters any partial publication and exits; no independent activation or fallback is introduced. No externally dispatched capture work can bypass the live channel/native checks. Readiness means initialization consumed, never capture permission.
4. Initially locked/locking can publish and complete Ready while every capture request remains denied. Later lock/unknown/content loss uses unchanged existing job revocation and lifetime fences. Owner loss before initialization prevents publication/Ready; owner loss afterward retires authority through existing channel/current-owner fences. A historical initialized bit must never be sufficient for publication after a generation/owner transition.

This tightens when the actual service/Ready becomes observable; it does not infer the cause of prior intermittent Screenshot refusals. It uses the same authenticated receipt path as production admission and keeps QCC1 ABI stable. Update capture reference/ADR0324 consequence and fork WIRE timing text by coordination (fork production source untouched). A distinct ADR is necessary only if review concludes the existing Ready timing contract cannot be strengthened append-only; no new wire type is proposed.

## Executable public-boundary regression plan

Add a focused separate broker-startup test, using a private bus and the real noninstallable backend executable (production main/AuthorityCapture/NativeCaptureAdmission), a parent-owned seqpacket fd3, actual parent PID credentials/pidfd, and a narrow NativeLock service owned by that same current canonical unique sender. The service implements targeted receipt and delayed empty method reply independently; assertions observe public name ownership and actual QCC1 packets, not private fields or test-made booleans.

- Send real Hello. Wait for actual RequestStateWithReceipt arrival. Withhold both pieces: backend service remains absent and no Ready packet exists. Original eager startup is the negative control: it publishes/name and Ready while these pieces are withheld, so this row must fail on exact ca299.
- Receipt first, method reply held: no publication/Ready. Reply first, receipt held: same. Release the second valid piece: backend publishes its actual name, emits unchanged Ready with its authenticated unique sender, and accepts ordinary unlocked admission on the same state. Wrong nonce/sender/malformed reply cannot complete readiness.
- Revoke canonical owner while one piece is withheld; late old-owner completion never publishes/Ready. Actual child exit/cleanup audited; no owner-string-only replacement is trusted.
- Initial locked or locking receipt completes initialization/Ready but actual Screenshot call is denied before any StartJob/helper. This explicitly distinguishes availability from pixel authority.
- Preserve focused authority codec/channel and capture request/privacy tests, then original native matrix only under a new granted lease. No arbitrary startup sleep or retry; bounded waits are for actual messages/receipts/name transitions. All PR0/core0/fatal-warning/FD roles stay intact.

The two-phase startup must be checked for reentrant callbacks and partial registration cleanup. Startup timeout must remain bounded by existing native receipt/channel/compositor launch deadlines; no extension is planned. Exact tests will assert no output before required messages rather than relying on elapsed time as readiness.

## Ownership/resource request

Proposed narrow production paths: capture/native_capture_admission.{h,cpp}, capture/backend/authority_capture.{h,cpp}, capture/backend/main.cpp; focused capture tests/CMake and directly affected capture ADR/reference. No Channel protocol or fork production mutation planned. This record is the requested pre-edit boundary/acceptance proposal, not implementation acceptance. Root holds sole full production fork compiler; no build/private lease held here. Ready for manager routing/ownership confirmation before source work.

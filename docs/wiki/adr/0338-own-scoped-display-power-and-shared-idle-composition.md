# ADR-0338: Own scoped display power and complete shared idle consumers

- **Status:** Proposed; source delivery candidate, runtime gates pending
- **Date:** 2026-10-02
- **Supersedes:** ADR-0319's unconditional all-output On restoration
- **Related:** [ADR-0333](0333-authenticate-shared-idle-consumer-registration.md), [ADR-0321](0321-supervisor-owned-native-sleep-admission.md), [Idle policy](../architecture/idle-policy.md)

## Context

Raw DPMS reports effective mode, without writer provenance. An external Off can
arrive while our Off is already effective; a same-mode no-op erases that fact.
Client-side mode guessing cannot safely restore own blanks. Shared Power1 scopes
also require actual automatic-lock, display/dim and idle-sleep consumers, with
lid ScreenOff using the same display authority rather than a competing writer.

## Decision

Add one compositor-owned DisplayPower1 adapter over the existing Workspace DPMS
route. Record external requests before upstream's same-mode early return.
Finite owned causes compose with the recorded external Off; own release never
turns a preexisting or later external Off into On. Existing physical wake retires
owned episodes. Global upstream DPMS can only admit complete known output
coverage; an unexplained mixed physical baseline fails closed. Output identities,
logical geometry, capability and physical modes arrive in nonce-correlated
receipts from the selected actual compositor owner/epoch.

Only the current actual Session1 sender/UID can enable native admission or add
causes, while PowerDevil is absent. Admission starts false. Each cause has a
bounded absolute same-host monotonic deadline; delayed acquisition/renewal cannot
outlive its original deadline. Same-connection release ordering and owner loss
are primary cancellation fences; bounded canceled-ID retention closes the
cancel-before-delayed-acquire race. This is display-specific state, not a general
lease framework. Retirement releases only owned causes through the retained
unique peer, with bounded final acknowledgement and no replacement reconnect.

The supervisor owns a small two-cause facade: idle and persistent lid ScreenOff.
ScreenPower1 shares Session1's constructing connection/owner and accepts only
current Power1 callers/epoch; the original caller/ID can release its episode.
The existing native runtime's actual Protected callback gates lock-before-off.
Method-reported acceptance means cause admission, never physical blank proof.
Physical mode and operation failure are distinct observations.

Shared automatic-lock/display/dim/suspend consumers use authenticated Power1
scope receipts. Genuine current-generation compositor resumed events identify
activity; startup, rearm and lost availability cannot manufacture it or replay a
consumed idle episode. Lid reopen/source/admission/owner loss cancels only its
cause, and a physical wake cannot reblank a continuously closed lid.

The native-exclusive candidate wires these consumers through NativePowerComposition.
`QINDAQT_NATIVE_POWER_EXCLUSIVE=ON` configures all four Power1 native-exclusive
activation flags and the supervisor exclusive default, omitting only its owned
PowerDevil child. An explicit supervisor off override retains development behavior.
Idle cancellation uses the existing SleepCoordinator's exact request token, so
canceling a stale idle request cannot cancel a later manual request.
Dim restores only authenticated unchanged own values after mutation readback;
external/manual changes and uncertain writes remain outside its ownership.
Bare development binaries remain default off. Manager package cutover follows
independent review and integrated gates; no host hardware action is a source gate.

## Verification and remaining boundary

Private-bus tests cover real caller credentials, owner replacement, receipt
spoofing/nonce correlation, incomplete inventory and cancellation before delayed
admission replies. Compositor policy tests cover external same-mode/preexisting
Off, independent causes, physical wake, expiry and bounded rejection. Actual
renderer/output integration, shared inhibition and lid/source/dock rows remain
required. This source checkpoint is not completed native power retirement.

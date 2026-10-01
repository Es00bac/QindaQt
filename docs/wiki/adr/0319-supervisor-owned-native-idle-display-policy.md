# ADR-0319: Compose native idle display policy in the session supervisor

- **Status:** Accepted
- **Date:** 2026-10-01
- **Amends:** the idle-stage process location in [ADR-0306](0306-power1-idle-inhibitors-and-stage-policy.md)
- **Related:** [ordinary compositor attachment](../architecture/compositor-attachment.md), [native idle display stage](../architecture/idle-policy.md), [native lock](../architecture/native-session-lock.md)

## Context

ADR-0306 placed idle policy in Power1, while the program plan originally put
the idle engine in desktop-controls. Native Lock1 now composes selected session
identity, admitted ordinary compositor descriptors, actual idle observation and
protected-state receipts in qindaqt-session. A second process that reopens an
environment-derived Wayland path would duplicate that admission/lifetime
boundary and could accidentally act on a replacement compositor.

## Decision

Keep the transport-independent stage core and pure confirmed per-source
Settings1/Power1 selection in session/idle_policy. Compose its production
ordinary display-power port in qindaqt-session beside NativeLockRuntime. Each
stage has its own idle observation; display-off changes never overwrite the
automatic-lock timeout. The composition borrows the public attachment,
SettingsClient and PowerClient boundaries; KWayland DPMS implementation stays
private to IdlePolicy. No compositor-private API crosses the process boundary.

Power1 remains the single inhibitor registry and actual power-state authority.
Scope admission stays atomic, bound to exact caller/epoch lifetime and disabled
until complete consumers are qualified. This process-location amendment does
not advertise any scope, implement idle suspend or dimming, or relax Protected
before QindaQt-owned suspend. Complete native stage capability composition
remains a later boundary.

Activity restores display power. Attachment revocation disarms observation and
makes one best-effort final On request through the already-retained peer FD.
The connection worker flushes before protocol wrappers/display are destroyed.
Final restoration intentionally survives loss of admission for that retained
peer; it never reconnects a socket path or sends a request to a replacement.
Connection death permits only local proxy cleanup.

PowerDevil remains an optional transitional child for other desktop behavior.
Removing desktop-controls' preference binding does not disable independently
configured legacy PowerDevil idle policy. PF2–PF4 must retire that policy and
child before claiming a single production display-power authority. This slice
therefore adds native source behavior and does not complete PowerDevil removal.

## Consequences and verification

Pure stage tests must cover current-owner preference/source selection, disabled
and unavailable inputs, actual activity restore, unknown Power1 receipts,
DisplayOff lease suppression and attachment revocation. A real private Wayland
DPMS server must exercise asynchronous capability, per-output requests, output
removal, reconnect, final flushed On and repeated teardown with fatal Qt
warnings. Nested compositor and physical display cycling remain separate gates.
The Settings route proves per-source bounds and exact commit/readback truth;
schema migration retains stored legacy minutes without inventing absent values.

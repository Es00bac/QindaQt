# ADR-0373: Separate Android renderer origin from guest app identity

- **Status:** Accepted
- **Date:** 2026-10-09
- **Owners:** Android Runtime and Compositor
- **Supersedes:** None
- **Superseded by:** None

## Context

A native Wayland window's kernel client PID can identify its live host renderer.
It cannot identify the Android package or task rendered inside that process.
An admitted app-catalog collector may run in a separate broker process; matching
its inventory to an unrelated window by title, app_id or numeric PID guesses
would manufacture platform origin and could lend application control authority.
[ADR-0372](0372-bind-private-android-producers-to-kernel-lifetimes.md) already
fences a provisioned producer's kernel credentials, executable and lifetime.
The next compositor boundary must retain that distinction without prematurely
patching the window registry or public facts publisher.

## Decision

Add the Qt Core `QindaQt::AndroidRendererEvidence` compositor target and a public
AndroidRendererEvidenceSource boundary. Its NativeWindowPeer is copied fresh
from a live compositor-managed window: canonical window incarnation, native
Wayland flag and the kernel-derived wl_client PID. A client-supplied PID property,
X11 window, title, app_id, process argv or a public bus owner is not this input.
The consumer gate checks the exact returned window/PID binding and canonical
runtime/renderer incarnations. Refusal means unknown, not a Native inference.

AdmittedRendererDescriptor is the deliberately narrow in-process renderer case.
Trusted bootstrap must already qualify the exact provisioned producer process
as the native renderer for one runtime, generate a non-reused renderer
incarnation and keep the borrowed DescriptorRuntimePort alive on its Qt thread.
A separately admitted broker/collector must never be supplied as this renderer
just because it published app metadata. Separate-renderer lifetime/bootstrap
composition needs its own adapter rather than widening this match.

The descriptor port exposes authenticatedPeer only after a valid private-key
packet with actual kernel sender credentials, while its held pidfd/executable
remain live and unchanged. Observation checks exact producer PID, runtime
attachment, Ready/Starting availability, then rechecks the process fence after
copied snapshot getters. Channel/process loss, runtime replacement or unavailable
status withdraws evidence; no queued death notification is required. Callers
observe again rather than caching an authority-bearing result.

The result carries only host platform origin: window incarnation, runtime
attachment, renderer incarnation and kernel process PID. It contains no AppKey,
package, guest UID, task, surface mapping, stop permission or app window count.
An authenticated guest collector and separately checked package/task/surface
association are still required before ordinary app-window ownership publication.
This static additive C++ API requires consumer rebuilds; protocol version1 and
public wire formats are unchanged. Compositor depends on the Android transport;
no KWin dependency enters the Qt Core protocol or service transport.

## Consequences

The isolated target and owned-fork tests are executable now without a guest,
KWin process or display. They verify unqualified/X11/wrong-PID rejection, exact
provider binding, private credential admission, immediate pidfd death, runtime
withdrawal and replacement. These are inert synthetic publishers, not Android
apps or live renderer proof.

Existing registry, publisher, supervisor and production composition are untouched.
There is no automatically assigned origin, default green window badge, app-tab
claim or native/Android mixed-container proof from this change. The management
entry remains NoDisplay with null runtime until separately admitted bootstrap,
materialization and collector composition is qualified. The next integration
must request the exact registry/publication seam and preserve window-container
invariants. The [Android Applications page](../apps/android-applications.md)
records this bounded source outcome and next gates.

## Revisit when

A verified production renderer is composed separately from the collector, or
an authenticated guest collector can prove package/task/surface association for
ordinary QindaQt containers without transferring guest-wide authority to an app.

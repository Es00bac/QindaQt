# ADR-0374: Bind separate Android renderers to held lifetimes

- **Status:** Accepted
- **Date:** 2026-10-09
- **Owners:** Android Runtime and Compositor
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0373](0373-separate-android-renderer-origin-from-guest-app-identity.md)
deliberately covers only an already qualified in-process producer/renderer.
A stock runtime can have a collector/broker and native Wayland renderer in
separate processes. The collector's authenticated PID must not be substituted
for the renderer's kernel client PID. Both lifetimes and the runtime attachment
must remain present for host platform evidence to survive.

## Decision

AndroidRuntime owns HeldRendererLifetime. Trusted bootstrap transfers distinct
owned pidfd and already admitted executable descriptors held from observed
renderer birth, plus the exact host PID in the compositor's PID namespace. These
values never come from public argv, a client PID property, guest PID metadata or
an untrusted message. The holder verifies pidfd identity and pinned executable,
rechecking live pidfd and executable device/inode/size/mode/timestamps on every
getter. Observed loss permanently retires this holder. Destruction closes owned
descriptors without stopping or killing a process; access/destruction is serialized
on the owning thread. The transport and renderer holder share one private verifier.
This checks the provided process/object; installed-code and renderer-role admission
remain trusted bootstrap responsibilities.

RuntimeRendererBinding borrows an authenticated DescriptorRuntimePort collector
and independent renderer holder, both of which outlive it. Calls stay on the
collector's Qt thread. Bootstrap qualifies this renderer for the expected runtime
attachment and supplies a non-reused renderer incarnation. Observation requires
collector credential/key admission, exact current Ready/Starting runtime attachment
and the held renderer PID, then rechecks both process fences after copied getters.
Collector channel/death/exec loss, renderer death/exec loss, runtime withdrawal or
attachment replacement returns unknown. A copied inventory cannot qualify a
renderer by itself.

The service's copied result contains runtime/renderer incarnations and host PID,
with no compositor pointer, AppKey, package, guest UID/task/surface mapping,
window count or app-control permission. QtCore protocol/transport remains free
of KWin dependencies and protocol version1 is unchanged. This additive static
C++ boundary requires consumer rebuilds.

New KWin-only android_native_window_peer helpers copy a current live,
non-internal, managed WaylandWindow internalId and wl_client kernel PID on the
compositor thread. X11, absent/deleted/internal/unmanaged windows or invalid
incarnations/PIDs refuse. The split-renderer observer then passes this exact
peer through ADR-0373's existing public evidence gate. No title/app_id/resource
class, PID property or public bus name participates. Helpers retain no window
pointer and are not a cached authority.

## Consequences

The new native extraction object is included in the plugin source list and
compiled separately against the release-matched installed KWin headers. Existing
registry, publisher and supervisor implementations are untouched and do not call
it yet. No automatic origin badge, app association, guest count or mixed native/
Android container journey is claimed. Production management remains null-runtime
and NoDisplay until qualified bootstrap/materialization/collector composition.

Short owned-fork tests distinguish actual collector and renderer PIDs, reject
wrong pidfd/executable objects and duplicate ownership, fence immediate death and
same-PID exec change, and withdraw on collector socket/death or runtime replacement.
They reuse inert synthetic publishers, not Android or a live compositor. The
[Android Applications page](../apps/android-applications.md) records next gates.

## Revisit when

Verified renderer bootstrap and authenticated guest package/task/surface evidence
are available for an exact coordinated registry/publication hook and ordinary
QindaQt window-container coverage. Guest origin and control authority must remain
separate from this host platform/lifetime evidence.

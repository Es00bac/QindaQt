# ADR-0361: Guard owned send gain and producer lifetime

- **Status:** Proposed
- **Date:** 2026-10-08
- **Owners:** Audio service
- **Supersedes:** None
- **Superseded by:** None

## Context

The full Audio source audit at tested cohort 5b7f found that strip and bus
faders were persisted but absent from declared routing. The worker sent the
desired linear send gain directly to a cubic WirePlumber mixer. A minus 6 dB
send therefore requested approximately minus 18 dB in the underlying Props.
Model echoes and configured operation success did not establish audible gain.

Independent plus 12 dB strip, send and bus stages can produce plus 36 dB of
headroom. Legitimate internal amplification must neither be silently clamped to
a public normalized percentage nor cause the entire public Audio graph to fail
validation. Name prefixes, copied tokens, mutable PID properties and partially
matched channel arrays cannot grant private graph ownership.

## Prospective decision

The private gain collaborator multiplies the three separately admitted
amplitudes, then the existing stereo pan balance. Each stage retains its own
minus 60 dB bottom-stop as exact silence. The audibility flag combines enabled
send, strip mute, solo eligibility and bus mute. A sum clamped to one fader
range is forbidden. Mono and per-channel trim remain separate required work.

Only admitted internal send writes invert the cubic mixer scale with a cube
root. Ordinary device/app percentage controls retain their current scale and
strict normalized validation. Private playback headroom is derived from the
three maximum stages, rounded upward to the PipeWire float boundary. This
configuration is a declaration until raw Props and actual low-amplitude PCM
qualify the implementation.

### Actual producer and module admission

The worker must observe its own connected raw Core client ID and that client's
daemon-global serial, together with a nonzero worker generation. One owned
module declaration creates a bounded fresh instance/token and exact capture
and playback names. Both actual node objects must match that producer, role,
media class, name and token. Admission pins each object's serial, bound ID and
local proxy identity. Duplicate roles, loss, serial/object replacement,
retirement or producer change revoke writes; old tuples cannot reacquire.

Only those pinned Stream/Input/Audio and Stream/Output/Audio objects may be
excluded before ordinary public volume mapping. Public Audio/Sink and
Audio/Source console endpoints remain visible and routable. External devices
and streams retain the unchanged strict mapper/validator. Retiring pinned
objects may remain hidden while module removal is pending, but cannot be
written or rebound. Before projection, the worker must quiet and retire
invalid modules. Marked observation overflow returns no partial prefix.

### Scoped borrowed raw Core

The actual compatibility reference is installed WirePlumber 0.5.15 and
PipeWire 1.6.8, not the newer online Core manual. PipeWire loopback borrows a
Core found in its context; without that slot it opens a different client.

A same-worker ScopedContextCore inserts its connected raw Core only for the
fixed synchronous loopback factory call. It drives no event loop. Any existing
slot, including the same pointer inserted by another owner, refuses admission.
Nested scopes refuse and leave the outer slot untouched. Successful insertion
is remembered separately; release removes only that exact inserted pointer on
success or failure. Changed foreign occupancy is never cleared by this owner.
The returned module must be destroyed before its context, and writes require
a fresh current producer/object observation after the slot has gone.

WirePlumber 0.5.15 emits disconnected from its raw proxy destroy callback after
PipeWire's earlier Core callback already tears down streams and internal
resources. Persistent context registration removed from that signal is too
late. The proposed scope also references raw proxy memory and removes its own
entry in raw destroy; this protects against a dangling slot at final proxy
free, not against earlier internal teardown. It never keeps a disconnected
Core operational. Normal asynchronous destruction happens with the slot
already absent after load. Source/native review must establish the fixed
factory's no-dispatch contract and subtractive disconnect/nested/destruction
paths before accepting this prospective boundary. A memory reference alone is
not acceptance evidence for a live Core.

### Required upstream loopback listener lifecycle

The frozen upstream PipeWire 1.6.8 loopback registers both Core and Core-proxy
listeners in module-owned impl storage. In its borrowed-Core path,
impl_destroy frees that storage without detaching those listeners, while the
shared Core remains live. A later Core event/disconnect can traverse freed
hook storage. Scoped context registration and an extra proxy reference do not
repair that lifetime. This is source-traced, not a reproduced native failure.

The prospective same-Core experiment and worker integration therefore require
a separately reviewed Portage-owned upstream listener cleanup. Both ordinary
module destruction and Core destruction must detach their own hooks before
impl storage can die, including deferred/auto-destruction. No worker reaches
into module-private structs or performs a home plugin installation. Native
shared-Core churn/disconnect, ASan, full affected package gates and manager
acceptance must qualify that dependency first. No borrowed-Core experiment is
currently authorized on the unchanged installed dependency.

The bounded Portage source candidate is PipeWire1.6.8-r1 from the QindaGentoo
worker branch at b489a5f8ce215da9fad5f5f10db0adbfa22af2a4; different-author source review accepted the bounded patch/recipe. ASan,
full package and installed gates remain pending. A future gain-enabled desktop
recipe must require exact =media-video/pipewire-1.6.8-r1::qindaqt for both build
and runtime before enabling borrowed-Core wiring. A generic >=1.6.8 floor or
the upstream runtime version string cannot prove the downstream hook patch.
The actual loaded Core library/module must match that Portage-owned qualified
image, with no ambient module-path bypass. Root owns the later coordinated
desktop dependency/enablement packet; this ADR does not claim that guard or
production worker wiring has already landed. Broader version compatibility
requires new lifecycle qualification and an explicit dependency revision.

### Diagnostic package-image admission

The source-only diagnostic now requires a configure-pinned receipt for an
actual retained Portage image and complete gpkg payload round trip. Normal
images require Portage signature verification. Prepared module source,
reviewed recipe/archive, actual Core/module/daemon/config files and image
hashes are pinned; the upstream version string alone is insufficient.
Before private startup or Core construction it checks the loaded Core symbol
and mapped inode. Fixed public factory loads require a synchronous non-reentrant
image gate before slot insertion and again after subtractive hooks, then prove
actual module filename and mapped image. Missing proof refuses load. A post-load
image mismatch aborts only the fresh private diagnostic group; its recipe kills
any remaining owned children and preserves the failure.

Bootstrap module/SPA/library/sanitizer paths are set before executable entry.
The load helper never rewrites loader environment or copies the private loader;
its factory name remains libpipewire-module-loopback. Native receipt checks
observe stable file identity/bytes; they do not exclude concurrent writers.
The manager must keep the qualification image unchanged during validation.
The unmodified upstream image is accepted only by an explicit ASan-red-control
build, never by the normal/patched build or a production worker.

The executable owner-local recipe retains Portage images using explicit ebuild
build/test/install-to-image/package phases, never merge/preinst/postinst. The
separate package verifier uses Portage's complete signed gpkg validation and
compares the entire extracted payload. The first actual unpatched ASan build
reached configure, where its compiled Meson sanity binary exited1 because
ASan's runtime was behind Gentoo sandbox's preload. This was a setup failure,
not the original module lifetime red control. A later fresh original ASan
cohort completed compile and all51 upstream observations, returning45OK/
5LeakSanitizer FAIL/1context TIMEOUT. It remains failed, with no image or native
control. The bounded RCA preserves exact identities/logs and treats DBus
allocation accounting and fork/wait explanations as inference. Fixed ASan and
normal test/release image variants remain unexecuted. Original-red acceptance requires
actual module heap-use-after-free evidence, not compilation failure or a guard
assertion.

A private feature-policy collaborator removes only sandbox/usersandbox in the
ASan branch beyond the existing diagnostic feature changes, following
[Gentoo's sanitizer interposition
policy](https://wiki.gentoo.org/wiki/AddressSanitizer#Per-package). Normal
sandbox/signing and retained network/pid/ipc/userpriv policy are unchanged.
No persistent Portage configuration, ASan link-order suppression or ambient
preload is introduced. The input collaborator copies exact already cached
Manifest bytes through bounded no-follow regular descriptors into a protected
root-owned0700/files0400 snapshot and an independent per-cohort writable
root:portage0770/files0664 DISTDIR. Portage never receives the caller cache or
protected directory as DISTDIR. This accommodates observed cache permission
normalization without weakening the strict image/artifact admission contract.
The final archive receipt uses the protected snapshot. Initial/final observed
hashes and protected identities are retained even if the build fails; mutation,
symlinks/hardlinks or protected identity changes refuse publication. These are
stable observations, not concurrent-root-writer exclusion. The finally-owned final observation covers
raised spawn/reap/log/record errors as well as ordinary failed returns. A
secondary verifier/evidence-write failure preserves the original exception and
cannot produce a usable receipt; evidence storage failure can prevent the
final file. Both Manifest read opens are nonblocking before descriptor checks,
including observed-name replacement with a FIFO. The exacte704 repair
passed31 filesystem/policy/orchestration controls and six AST checks; that
helper evidence did not qualify a package or native lifecycle.
The explicitly requested original-only failed-suite policy is separate from
normal/fixed qualification. Default behavior still denies failed tests. Both
package and native drivers require --allow-failed-upstream-old-asan in old
mode; schema2 pins diagnosticOnly=true/upstreamTests=failed plus complete raw
evidence and final input observations. Unknown/missing flags, wrong mode,
contradictory totals, incomplete registry/results, changed files, compile
failure, outer timeout, uncertain cleanup or runner/record exceptions deny.
A per-test timeout inside a complete returned suite does not become an outer
timeout exception. Native admission requires a separate default-OFF compiled
old-only flag and rechecks actual observations, before any private Core/load.

One explicit compile invocation freezes the compiled registry, then exactly
one test invocation preserves full Meson JSON/text/JUnit and command/cleanup
evidence. Both exact absent bus values must appear in every actual row after
Portage ENV_UNSET; the parent environment alone is insufficient. Every other
configured unset remains. The previous lab omitted the session value and
cannot be retroactively admitted. Installed Meson logs the same environment
passed to its test subprocess; that source contract is recorded in the recipe
README and must be experimentally established in the next cohort.

Only already-admitted original failed evidence can add process-local noauto
to the explicit install/package transition. Inspected Portage doebuild.py2369
excludes recursive dependency phases under that supported feature;
phase-functions.sh558–565 creates .tested after success. The diagnostic
requires .compiled and absence of .tested before/after assembly. No retry,
invented marker, USE/test removal, test-fail-continue, upstream suppression or
implicit fixed/release bypass is permitted. Normal/fixed dependency,
signature, sandbox and green-suite requirements remain unchanged. No failed
lab resume occurs; any future cohort needs a fresh output and independent
source acceptance plus explicit execution grant.

Exclusive bounded raw snapshots retain registry/actual JSON/text/JUnit and
phase/input proofs. Schema2 is intentionally incompatible with the old
private schema1 receipt. Producer, Python consumer/bootstrap and native guard
pin the same diagnostic status and same-cohort publication input record.
Native checks use small dedicated evidence/phase/suite collaborators; byte
bounds do not claim parser allocation deadlines or writer exclusion. All
product gain, normalized public device/app validation, SDK and installed
boundaries are unchanged. The final85 helper controls and21 AST parses passed.
Private evidence IO, suite observer, phase admission and injected
folder/environment composition stay separate from the entrypoint and native
receipt guard. The initial entrypoint137-line shape failure is retained; its
composition extraction restores the function limit. A separate Qt-only strict
receipt fixture compiled the test and three actual parsers and passed 36 checks
with zero failure, skip or blacklisting. The complete-suite fixture data is
synthetic; actual upstream/package evidence and all image/ASan/lifecycle/PCM
targets remain unqualified. This parser pass grants no package/runtime
permission.

This diagnostic is not production enablement. The later guarded worker source
and release-owned admission policy are described below; they default OFF and
do not qualify the dependency or enable the unchanged installed host.

The prospective package dispatcher additionally requires cooperative
cancellation. Its signal handlers only latch a borrowed request; the public
owning runner checks before/after acquisition and during polling, then finishes
held-reservation group cleanup and reaping without raising from that handler.
Lost reservation denies signaling a potentially reused group, and primary plus
secondary cleanup errors remain distinct. Constructor-visible runner injection
threads the callback through all recipe phases without changing default
qualification or old-only noauto policy.

Attempted-build and cleanup/evidence budgets are separate 1800/90-second
cooperative intervals. Final observations/index/result-write are included in
completed duration and late/cancelled completion refuses. The result-file
snapshot never claims final success; post-write terminal evidence plus the
manager's actual held-child successful settlement/duration are required before
admitting this diagnostic image. No kernel I/O deadline or blanket descendant
emptiness is claimed. Pure injected process/signal/clock controls are separate
from an ungranted actual package/child/runtime gate; original c10 NeedsFix and
the failed 51 cohort remain immutable.
Final terminal print/flush and post-emission refusal preserve the exact
primary build/evidence object and its attached cleanup failures; diagnostic
sink errors are secondary. With no prior error the first terminal failure
remains authoritative. This is prospective diagnostic publication discipline,
not production gain or package qualification.

The owner now caps future private qinda package tasks at8jobs/load8 under an
existing transient service with eight-CPU quota, half-core affinity,12GiB/no
swap/tasks256/nice10 and runtime1800. Original24/24 plans remain preserved.
This resource gate can deny or interrupt the retained cooperative qualification
workflow; partial evidence, resource/unit failure or uncertain settlement
never qualifies an artifact. It changes no failed-old opt-in, test/signature,
sanitizer, marker, input or production admission policy. Laptop heavy work is
separately limited4/4 and at most5GiB; no grant follows this source preparation.

### Quiet initial admission

Candidate playback Props start muted with zero channel amplitudes and disable
policy stream-property restoration for the send. The worker must not release
quiet state until both actual roles and the current producer are admitted and
the actual mixer setter accepts a bounded gain. A refused setter cannot yield
running success. Creation, policy restoration and graph rebuild may not emit a
loud interval while waiting for those proofs. Argument text is insufficient.

## Verification and current limits

The source packet contains a separate portable private-loopback experiment:
isolated PipeWire and policy-only WirePlumber, explicit child environments,
pinned relative daemon/client names, private installed client/server configs,
hostile socket/config/notification overrides scrubbed in library composition,
private XDG roots, absent host
buses, fixed null endpoints and synthetic stereo F32 input. It never accepts a
host hardware target. Separate targets exercise gain math, admission, hostile environment, Core
scope lifetime, package-image preflight, real allocator failure after hooks,
owned/borrowed teardown/churn and raw Props/PCM. The precise executable
recipe and unexecuted matrix are in
`tests/services/audio_service/console_gain_experiment/README.md`.

Mandatory native cases include initial first-block silence with delayed setter,
a real saved-property restoration control, inverse cubic minus 6 dB and pan,
plus 12/plus 36 dB headroom, each bottom-stop with the other stages boosted,
mute/solo eligibility, public endpoint visibility, same-process different-client
copied identity, duplicate roles, serial replacement, refusal, disconnect
before factory, nested scope/module destruction, registration failure,
unexpected occupancy, repeated stop and immediate reconnect.

The Core/PCM private experiment targets remain unexecuted. The Qt-only receipt
parser gate above does not execute those targets. ConsoleModel publishes
stripGainDb and busGainDb alongside the
preserved per-send gainDb; the coordinator forwards those separate values and
compares them when deciding whether to republish routing. The production
worker now consumes these helpers through the default-OFF qualified route
collaborator; ordinary builds retain the legacy separate-client path. Declared
stage propagation and injected setter acceptance do not prove audible gain
or quiet first-block startup.
Actual worker public-operation and two-output/bus-processing PCM regressions,
immutable 5b7 old-source controls, owning production build, installed-only SDK
header poison/restoration, independent review and manager integration remain
required before the user-visible gain repair can be claimed. Fresh-login and
physical-device/fullscreen/game qualification are separate later gates.
No Audio1 schema, normalized validator or device-volume mapper is relaxed.

### Guarded product source and release enablement

`ConsoleSendReconciler` owns the bounded route declarations, module instance
admission and retirement policy. The GLib worker adapter resolves configured
strip/bus endpoints, performs the guarded synchronous loopback factory call,
checks the actual loaded image and writes only exact current admitted node
tuples. Independent strip/send/bus amplitudes enter the inverse cubic mixer
setter; actual setter refusal withdraws that route. Retirement quiets only a
still-observed former tuple, schedules module destruction outside dispatch,
and waits for its exact destroy callback before replacing that matrix cell.
No name-only write or premature duplicate replacement is admitted.

The new source policy `QINDAQT_AUDIO_ADMITTED_SENDS` defaults OFF. An enabling
release recipe must require exact PipeWire1.6.8-r1::qindaqt and supply canonical
Core/module filenames plus their SHA256 digests only after the separately
qualified dependency is accepted. Configure rejects incomplete pins. Runtime
checks exact Portage revision/repository and pinned library versions, actual
Core mapping inode/bytes, pre-entry `PIPEWIRE_MODULE_DIR`, module bytes and
actual post-load module mapping. These stable observations do not exclude
concurrent privileged writers; release qualification keeps its inputs stable.
No runtime option, generic version floor or upstream version string replaces
that proof. The fixed dependency recipe exists at the source candidate named
above; it has not been installed or qualified by this product slice.

Owning short fixtures use the real reconciler with injected producer/node and
setter outcomes. The real Audio1 server/Qt transport/client fixture reads back
three independent application-strip gains and sends to two explicitly pinned
outputs on a fresh private D-Bus. The streams and graph are synthetic; no
PipeWire process, host graph/default change, native/Android/Windows runtime or
PCM qualification is part of those gates. A preserved prerequisite missing
gain-header include in `console_document.cpp` was found by focused compilation
and repaired in this same source packet.

The public round trip also exposed a pre-existing pin result bug: synchronous
console republish advanced the revision before the coordinator constructed the
initiating lineage. It now freezes admission epoch/revision before applying
the console change, retaining the newer observed lineage. The public client's
handle-targeting validator is unchanged. The fixture waits for that observed
snapshot before its next pin instead of treating operation completion as
already-delivered readback.

## References

- [Audio service](../architecture/audio-service.md)
- [Console parity target](../reference/voicemeeter-potato-parity.md)
- [Module boundaries](../architecture/module-boundaries.md)
- [PipeWire 1.6.8 loopback source](https://github.com/PipeWire/pipewire/blob/1.6.8/src/modules/module-loopback.c)
- [PipeWire 1.6.8 Core/proxy lifetime](https://github.com/PipeWire/pipewire/blob/1.6.8/src/pipewire/core.c)
- [WirePlumber 0.5.15 Core source](https://github.com/PipeWire/wireplumber/blob/0.5.15/lib/wp/core.c)
- [WirePlumber 0.5.15 mixer source](https://github.com/PipeWire/wireplumber/blob/0.5.15/modules/module-mixer-api.c)

# ADR-0372: Bind private Android producers to kernel lifetimes

- **Status:** Accepted
- **Date:** 2026-10-09
- **Owners:** Android Runtime and Session Supervisor
- **Supersedes:** ADR-0369's app ownership of RuntimePort/types only
- **Superseded by:** None

## Context

The Android manager needs an admitted RuntimePort producer before live app
registration or window facts can be projected. A public bus name, same UID,
claimed PID, title or launch acknowledgement cannot supply that admission.
Services also cannot depend on an application's implementation to own these
cross-process values.

## Decision

Move RuntimePort/copied values/validation to the Qt Core-only
`QindaQt::AndroidRuntimeProtocol` target under services/android_runtime. Preserve
the old app header as source-compatible aliases; all static-library consumers
rebuild together. The service transport depends only on that protocol and Qt
Core, never the Android app/GUI or another service's private implementation.

The transport consumes an unnamed private inherited SOCK_SEQPACKET descriptor,
a pidfd held from supervised birth, the already verified native producer
executable object and a private32-byte channel key. Trusted bootstrap must admit
the executable/session/runtime/recipe context before passing these objects. There
is no public argv/bootstrap mode or automatic GUI connection.

SO_PASSCRED is enabled before endpoints reach a child. Each packet's kernel
SCM_CREDENTIALS PID/UID must match the admitted process. SO_PEERCRED from a
socketpair created before fork identifies its creator, so it is deliberately
not the eventual child identity. Unexpected ancillary descriptors are closed
and rejected. Truncation, missing/duplicate credentials and wrong key/direction
withdraw the channel without logging packet/secret contents.

A held pidfd and unchanged executable dev/inode/size/mode/timestamps are rechecked
before publication and every getter/send/read, including before queued Qt death
notification. Numeric PID reuse, death or exec replacement cannot inherit the
old producer. No process is killed by this transport. Bootstrap still owns
installed-code attestation and actual guest package/task/surface evidence.

Version1 packets use explicit little-endian closed tags, bounded UTF-8 fields,
arrays and total64KiB. Length/count limits precede allocation; incomplete/trailing
or invalid snapshots never publish partial rows. This packet ceiling is stricter
than the public copied-value row bounds and refuses oversized contributions
rather than truncating them.

RuntimePort receiver and helper-side ProducerEndpoint carry copied status,
read-only refresh and exact revision/binding launch intents/results. Neither
launch dispatch nor result completion sets Running/window counts. Operation IDs
are retained without eviction (256 per instance), with64in flight and one per
app. Duplicate same-data IDs reuse retained results; changed-data reuse refuses
and never redispatches. Owner/registration loss reports OutcomeUnknown. Retired
runtime attachment IDs cannot reanimate after unavailable/replacement. Signal
publication does not retain mutable hash iterators across reentrant consumers.

## Consequences

This delivers actual private kernel-credential/lifetime transport and a helper
producer endpoint. It does not implement a Waydroid binder/guest collector,
materializer, supervisor startup composition, live renderer association or app
window authority. No guessed PID/metadata is promoted. Existing management
production remains null-runtime/NoDisplay until a separately admitted producer
is composed. No guest bootstrap, package action or VM is introduced.

Short native tests fork only owned inert fixtures. They verify actual packet
credentials, wrong-sender rejection, pidfd death before queued notification,
exec replacement, key/revision/schema failures and no replay. Synthetic snapshot
values are not Android package/window evidence. The owning
[Android Applications page](../apps/android-applications.md) records next gates.

## Revisit when

Supervisor bootstrap and verified recipe/materialization collection are ready,
a live Waydroid renderer must be admitted to compositor windows, or packet size/
platform requirements need a new version rather than an ambiguous extension.

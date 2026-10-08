# ADR-0359: Recover only the selected Bluetooth radio

- Status: Proposed
- Date: 2026-10-08
- Scope: explicit selected-adapter power enable and bounded diagnostics
- Partially supersedes: [ADR-0037](0037-keep-pairing-and-trust-authority-in-bluez.md) and [ADR-0057](0057-reach-bluez-through-direct-qtdbus-behind-adapter-backend.md), only their blanket exclusion of an owning rfkill helper

## Context

An ordinary login showed paired devices while the adapter was off. A typed
power-enable request returned a generic BlueZ error. The selected kernel radio
was software-blocked, but removing that block was not sufficient evidence of
request success: another request returned org.bluez.Error.Failed, and a later
independent snapshot reported Powered=true. The user then connected a speaker.
A method error, radio-block state and current adapter state are separate facts.

The main Bluetooth daemon's actual managed namespace cannot see /dev/rfkill.
It retains PrivateDevices and NoNewPrivileges, including unrelated installed
drop-ins. The active user's device ACL already permits access outside that
namespace. Root, KAuth or a global radio toggle would introduce broader authority.

## Decision

Add one optional, separately sandboxed qindaqt-bluetooth-radio-helper, activated
only for explicit selected-adapter enable. Existing user ACLs are the authority.
No capability, root process, polkit action, persistent radio policy or startup
write is added. Portage owns the binary, user unit and D-Bus activation data in
the existing desktop package. Direct D-Bus Exec fallback is disabled: lack of
systemd activation cannot silently run the helper outside its unit.

The main daemon keeps its sandbox. Constructor-injected public RadioPowerPort
has one observe-and-unblock operation. Policy, Qt IPC authority, Linux
descriptor/sysfs integration and presentation are separate collaborators.
The port is same-thread and borrowed for the backend lifetime; it returns a
local ID before asynchronous completion. Cancellation removes intent but cannot
undo an attempted syscall. The old backend constructor retains direct BlueZ
behavior for existing consumers.

One explicit enable admits one exact current helper owner and bounded request,
observes the selected kernel radio, and only if software-blocked and not
hardware-blocked attempts one per-index Bluetooth RFKILL_OP_CHANGE. After
identity/state readback, the backend independently re-admits its current target
before at most one BlueZ Powered=true call. Definitive no-write helper absence
or unavailable observation preserves legacy direct behavior without claiming
unblocked truth. Known refusal/block or possible-write uncertainty never takes
that fallback. There is no retry, rollback, reblock, CHANGE_ALL, hotplug/default
policy write, discovery, pairing or trust operation in the helper.

## Authority and identity

The bounded request contains a 32-character lowercase nonce, exact BlueZ unique
owner, canonical /org/bluez/hciN path, canonical address, initiating unique caller
and CLOCK_BOOTTIME deadline at most two seconds ahead. This wire is separate
from unchanged Bluetooth1/2. The helper accepts only the current Bluetooth1
unique owner on its constructing session bus under the existing user UID, and
checks the initiating caller still exists on that same bus.

A fixed read-only intent endpoint on the daemon confirms the **entire issued
request**, including arguments, caller and expiry, against its pending ledger.
It checks the current exact helper owner and backend lifetime. Recomputed nonce
or replacement arguments are not authority. Caller loss, cancellation, backend
stop, BlueZ owner loss or adapter retirement removes intent. A replacement
helper owner cannot reuse the old admission.

The helper addresses BlueZ Properties.Get to the captured exact owner and
requires the selected Adapter1 Address to match. It joins that path's kernel
HCI name to descriptor-pinned sysfs directories and the rfkill object's kernel
parent, requiring SYSFS_MAGIC and a unique Bluetooth radio. It neither invents
a sysfs address attribute nor trusts an IPC rfkill index. /dev/rfkill must be
character device 10:242. Enumeration and event drains are bounded. Initial ADD
inventory establishes state; selected deletion/re-add or identity replacement
retires the lease. A globally bounded ledger retains each exact (unique owner, nonce) pair across
well-known alias relinquishment and reacquisition, and expires entries only after
their deadline, when replay already fails; no old observation is current truth.
A full ledger refuses new requests rather than evicting unexpired entries.
Nested helper dispatch is refused before platform calls.

## Fences and limits

Authority, deadline, selected identity and state are checked around the syscall
and readback. Userspace observations cannot make a kernel write atomic with
hotplug, owner loss or permission changes. A short/error write is possibly
effective; missing/stale readback returns Uncertain without another write.
The two-second admission window is not a hard kernel execution-time guarantee.

Backend queued work, helper replies and BlueZ replies are bound to captured run,
owner, object path and address. Remove/re-add invalidates pending work even if
path/address are reused. Caller loss removes that caller's authority. Successful
method replies do not fabricate Powered; BlueZ property publication remains
snapshot truth. Error.Failed becomes uncertain power outcome, never a synthetic
RFKill diagnosis or proof of no effect.

Only fixed reason codes produce new UI messages: hardware/software block,
blocked-with-unknown-kind, authority refusal, stale target, busy, uncertain radio
change and uncertain BlueZ power result. No raw names, addresses or upstream
error messages enter those messages.

## Packaging and qualification

The helper user unit uses PrivateUsers, PrivateDevices, an explicit /dev/rfkill
bind, closed device policy for that node only, NoNewPrivileges, protected
system/home and AF_UNIX only. Existing ACLs remain authoritative: the unit adds
visibility, not privilege. Effective namespace and RW open require independent
qualification. The prior read-only namespace probe is not RW/mutation evidence.

Required gates include policy/replay/deadline negatives, private-bus
caller/current-owner/full-intent binding, real private BlueZ replacement and
late-reply cases, no-write fallback, fixed UI feedback, strict builds, staged SDK
closure and package/unit checks. Linux radio writes and ordinary installed
control remain separately authorized manager tests. This source draft is
uncompiled, not an installed repair or whole ED outcome claim.

## Sources and consequences

The [Linux rfkill API](https://docs.kernel.org/driver-api/rfkill.html) separates
hard and soft blocks; the [kernel implementation](https://github.com/torvalds/linux/blob/master/net/rfkill/core.c)
supports per-index CHANGE. A successful write alone is not proof of a matching
current radio. [BlueZ adapter source](https://github.com/bluez/bluez/blob/master/src/adapter.c)
maps several controller failures to Failed and RFKill-specific failure to
Blocked. These upstream references explain the boundary, not installed binary
version or live cause.

Pairing, trust, records, keys and authorization remain BlueZ-owned. Bluetooth
audio remains Audio/PipeWire-owned. See [Bluetooth service](../architecture/bluetooth-service.md)
and [module boundaries](../architecture/module-boundaries.md).

## Source review repair — owner reacquisition

The original source candidate cleared nonce history on admitted owner changes.
An A→B→A alias transition could therefore forget A's unexpired nonce inside the
operation engine. Current full-intent and completed-request retirement are
additional barriers; this was not demonstrated as an end-to-end radio replay.
The repair retains globally bounded exact-owner/nonce entries until deadline.
Direct injected-policy and actual private-bus alias transition fixtures exercise
the real engine; their native execution remains pending. The staged consumer
uses ordinary configured/default cmake build behavior, with no portable source
hard-coding of the manager's native parallelism settings.

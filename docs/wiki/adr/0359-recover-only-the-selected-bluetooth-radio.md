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
has one observe-and-unblock operation. Policy, sender-preserving IPC authority, Linux
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
and CLOCK_BOOTTIME deadline at most two seconds ahead, plus the issuing main
authorityOwner and its explicitly delegated transportCaller. This new helper
wire is (ssssstss), separate from unchanged Bluetooth1/2. The helper requires
the actual caller to equal the delegated raw connection and the full request
to remain issued by the current Bluetooth1 authority owner on the same bus.
Same UID is an additional restriction, never delegation by itself.

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
visibility, not privilege. Effective namespace and RW open require independent qualification. A separate
manager probe opened/fstat/closed the node O_RDWR under the named sandbox
settings without reading or writing any bytes. That qualifies access for that
transient unit only; the effective installed helper unit, per-index syscall and
ordinary control still require separate evidence.

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
Direct injected-policy and private-bus alias transition fixtures exercise
the real engine. The prior native batch passed those rows but failed transport
authority elsewhere; the revised native boundary must rerun the complete cohort. The staged consumer
uses ordinary configured/default cmake build behavior, with no portable source
hard-coding of the manager's native parallelism settings.


## Sender-preserving source repair — native qualification pending

The first native authority positive exposed a production transport defect:
Qt 6.11.1 QDBusMessage::service() always returns an empty string for reply and
error messages. The initial helper response checks therefore cannot admit
valid responses. Incoming method calls still expose the broker-assigned caller.

A proposed reliance on broker requested-reply enforcement was independently
rejected. The ordinary installed session configuration allows eavesdropping;
D-Bus 1.16.2 policy.c exempts those allow rules from requested-only filtering.
The library's pending-call lookup itself matches serial only. Neither a matching
serial/nonce nor a current name owner establishes who sent such a reply.
This proposal does not change broker policy.

Use an owning native libdbus transport with explicit sender, reply serial,
message type/signature and bound pending-request checks. The source package
already depends on D-Bus through Qt; direct libdbus-1 headers/link metadata must
be an explicit Portage/build dependency of this module and its static SDK
consumer. No Keyring private code is imported; its sender-preserving transport
is an architectural precedent only.

The public RadioServiceSession factory owns two connections to one constructing
Unix message bus: native transport first, then the main Qt service connection.
Read the authenticated native server GUID with dbus_connection_get_server_id and
pin the Qt connection's explicit address to that GUID. The documented D-Bus
address GUID and actual libdbus authentication check reject a different broker
incarnation between opens. Admit only a bounded, single Unix address; do not
guess addresses, fall back to another bus, or transparently reconnect. Failure
leaves the composed session unavailable. Only missing composition or an open
failure before any peer/GUID selection may preserve existing Qt-only startup;
the compatibility port is then definitively NoWriteUnavailable. A supplied
malformed address, selected GUID mismatch or subsequent connection loss never
permits that fallback. This owner outlives ResidentBluetoothService and
the borrowed radio port; its Qt connection remains the public Bluetooth1/2 host.
The executable composition captures the constructing address once. Startup
compatibility with activation and deterministic fixtures must be tested before
adoption.

The separate native caller is not the Bluetooth1 owner. Extend only the new
helper wire, before deployment, with authorityOwner (the main Qt unique owner)
and transportCaller (the native unique connection). The main owner records
these exact facts in the full pending intent before dispatch. The helper requires
the actual incoming native sender to equal transportCaller, independently resolves
current Bluetooth1 to authorityOwner, checks same-user/current initiating caller,
and asks that exact main owner to confirm the entire unexpired issued request.
The daemon's Current handler still authenticates its incoming helper sender
and captured helper owner. PID/UID or a caller-supplied owner string alone never
delegates authority. Nonce history is keyed by issuing authority owner, so
replacing a transport connection cannot clear unexpired replay protection.

The helper owns its native service connection and a native system-bus query
connection. Both use real dbus_message_get_sender checks for bus-driver and
captured unique-service replies; never infer sender from payloads. Maintain
independent exact serial/pending-call, full request, nonce, deadline, main-owner,
transport-caller, helper-owner and target-incarnation fences. A forged reply
that consumes a library pending call is refusal/Uncertain, not a reason to
reissue the operation. Native peer loss never creates no-write fallback after
dispatch. The radio port rechecks its borrowed current callback and re-finds
the pending entry after that callback before publication; callback cancellation
or reentrancy cannot revive an entry.

Native connection/message/pending-call references are RAII-owned. The public
SDK exposes no libdbus structs or private Qt headers. Each owner runs on its
constructing Qt thread; a bounded native socket notifier/pump delivers work
there. Deferred port completion is posted to a guarded Qt lifetime rather
than destroying an owner inside libdbus dispatch. Teardown disables notifier
and callbacks, cancels local pending admission, and closes/unrefs owned native
connections; it cannot undo an attempted radio write. Filter removal is paired
only with successful filter registration: authentication/Hello/GUID refusal may
leave an owned connection without a filter, and teardown must remain safe in
that partial state. Parsing and message queues
remain bounded. No second connection impersonates a lost name or automatically
reissues a request.

The alternative Qt internalPointer() is public-header-declared but documented
as internal and implementation-defined, with a borrowed pointer. It would
avoid delegation and reduce code, but impose a Qt backend/layout/version
contract and dispatch/lifetime coupling. Prefer the explicit owned transport
despite additional cohesive collaborators; do not silently cast that pointer.

Required new gates are actual helper/port complete success, a third connection
forging a reply from the real held request (correct serial and nonce), malformed
and wrong-nonce responses, duplicate/late responses, same-address/new-broker GUID
mismatch, wrong raw delegation, caller/main/helper owner loss, cancellation
during the borrowed callback, and the original authority negatives. A permissive
ordinary private session bus is the adversarial fixture, not a policy prerequisite.
Preserve original failed candidates and temporary diagnostics until these
replacement source and native gates are independently accepted.

Primary implementation evidence was read from official Qt v6.11.1 sources and
the existing Portage dbus-1.16.2 archive. See
[Qt message implementation](https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/dbus/qdbusmessage.cpp),
[D-Bus connection API](https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html)
and [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html).
The native source cache records archive/file hashes. The current source uses
separate private wire, codec, authority and service collaborators; its public
factory and port expose no native transport objects. The 25 ms bounded pump
also drains queued output and buffered messages when socket readability alone
would not progress them. New source and fixtures remain uncompiled until exact
independent review; this is not native or installed acceptance.

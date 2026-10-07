# ADR-0350: Share removable media with file consumers

- Status: Accepted
- Date: 2026-10-07
- Scope: ED-04a design; no public API or device-sidebar delivery yet
- Supersedes: ADR-0315 export-only-Activate restriction; all other decisions retained
- Accepted by: Program Manager after exact independent review `d283465c3b81bb50f694f5aa290f6443b567a412`; product API remains a future gate
- Source base: `7d45d2c336e5d213c7ba10f063ab384b09f4abc8`

## Context and existing evidence

The [Removable Media owner](../apps/removable-media.md) already discovers
eligible storage at startup and on insertion. [File Manager](../apps/file-manager.md)
currently browses mounted directories without discovering devices. The
[native chooser](../reference/portal-choosers.md) has a local folder field and
file tree; it does not consume a media inventory. Dismissing a notification
must not make attached storage disappear from those consumers.

These are exact source anchors, not observations of physical hardware:

| Existing responsibility | Source anchor at the base |
| --- | --- |
| Singleton compatibility: only `Activate` is exported | `src/apps/removable_media/main.cpp`, `Activation` and singleton registration |
| Startup publication after adapters attach; one prompt per token; ignored choices and serialized automatic mounts | `src/apps/removable_media/media_controller.cpp`, constructor, `inventoryChanged`, `nextAutomaticMount`; `tests/tst_media_policy.cpp::startupAndInsertionPromptOnce` |
| Dismissal drops only the notification mapping; absent/error/replaced notification owner opens the owner window; withdrawn late id is closed | `src/apps/removable_media/media_notifications.cpp`, `notify`, owner watcher, `withdraw`, `closed`; `tests/tst_media_notifications.cpp` |
| Eligibility, separate partition rows, encrypted backing exclusion, private identities and remembered-choice keys | `src/apps/removable_media/media_types.cpp`, `projectVolumes`, `physicalMediaIdentity` |
| Immediate revocation on owner loss, object removal or identity change; late inventory reply fenced | `src/apps/removable_media/udisks_backend.cpp`, `ownerChanged`, `fetch`, `interfacesRemoved`, `propertiesChanged` |
| Fresh admission, all sibling unmounts including hidden volumes, encrypted Lock, then Eject/PowerOff; no force/replay | `src/apps/removable_media/udisks_operations.cpp`, `execute`, `prepare`, `runNext` |
| Operation completion precedes independent inventory refresh today | `src/apps/removable_media/udisks_operations.cpp`, `finish`; no public converged snapshot/result protocol exists |
| Canonical local chooser paths and unchanged standard portal result validation | `src/services/portal/choosers/file_chooser_dialog.cpp`, `navigate`, `selections`, `accept` |

Existing executable fixtures exercise synthetic objects, temporary choice
files and private buses. They do not establish USB, optical, physical eject,
polkit interaction or destructive formatting qualification. Exact commands,
counts and any unavailable rows accompany the candidate handoff; merely naming
a fixture is not a passing execution claim.

The October 7 feasibility run compiled these unchanged owning sources in an
ignored standalone Debug harness with configured Portage limits and strict
warnings. Policy, UDisks and notification fixtures plus two disposable
notification-discovery cases passed four CTest rows: 30 Qt checks including
init/cleanup, zero failures/skips. The additional cases observed that missing
notifications request the existing owner window without mounting, and dismissal
keeps inventory and explicit window activation while rejecting the dismissed
id's late mount action. Private buses used only fixture UDisks/notification
objects; system-bus address was unavailable. This is owner-feasibility evidence,
not proof of the proposed public client, real GUI presentation or hardware.
Commands and exact ignored harness hashes are in the candidate handoff.

## Decision

Keep the running Removable Media application as the sole session owner of
inventory, insertion policy, saved choices, helper UI and private UDisks
operations. Extract no UDisks code into File Manager or the chooser. Add a
small public protocol/value module and an independent client; the owner adapts
its current private backend to those values. Consumers receive constructor-
injected clients, never the private `MediaController`, `Volume`, `Request`,
UDisks paths, preference keys or media-choices file.

Retain service `org.qindaqt.RemovableMedia1`, existing object
`/org/qindaqt/RemovableMedia1` and its unchanged `Activate()` interface.
Add object `/org/qindaqt/RemovableMedia1/Devices` with interface
`org.qindaqt.RemovableMedia1.Devices`, protocol version 1. Existing launchers
continue to work with old and new owners. An old owner lacking the Devices
object produces explicit unsupported inventory in new consumers; consumers
still browse ordinary local directories and may offer the existing helper's
Activate action. There is no second watcher, compatibility mount utility or
fallback UDisks consumer. The public client observes the owner without bus
activation: opening a browser/chooser must not start the insertion owner and
thereby run remembered mounts. The supervisor still owns the optional child;
explicit user launcher activation remains separate from observation.

Missing-owner recovery is graphical: the device section shows **Media support
unavailable** with a deliberate **Start media support** action. The public
client receives a constructor-injected `MediaOwnerLauncher` port whose closed
request is the existing `org.qindaqt.RemovableMedia.desktop` owner entry,
resolved through the public application-launch boundary. Consumers cannot
substitute a command or spawn mount tools. One explicit attempt may launch the
ordinary owning helper and its existing singleton policy; only observed owner
and valid inventory readback change the section to ready. A returned process
id or launcher success is not inventory readiness. Missing descriptor, launch
failure or bounded startup timeout remains visible with **Try again** and an
ordinary application-launcher route to Removable Media. If the owner exists,
Try again only rereads its inventory; unsupported old owners offer **Open
Removable Media** through unchanged Activate. No terminal, automatic restart
loop, hidden repeated launch or readiness assumption is part of recovery.
Private fixtures inject a recording launcher and never start the host helper.

This decision supersedes only ADR-0315's **export only Activate** restriction. Its UDisks, insertion, persistence and formatting decisions remain.
No formatting, passphrase, repair, partition editing or privileged command API
is added. Existing session-polkit authorization continues to apply to ordinary
owner-issued device operations; this same-user desktop API does not claim
sandbox caller attribution or executable attestation.

### Ownership and dependency direction

| Proposed owner | Responsibility and public boundary |
| --- | --- |
| `src/services/removable_media_protocol` | Typed immutable values, admission/result enums, strict canonical byte codec and shared bounds; Qt Core plus serialization-only Qt DBus, no connection, filesystem or device access |
| `src/services/removable_media_client` | GUI-thread asynchronous exact-owner transport, validation, atomic snapshot publication, one pending client request, uncertainty and no replay; protocol plus Qt Core/DBus, no UDisks/helper policy or presentation |
| `src/apps/removable_media` | Private protocol exporter/admission adapter over the existing owner backend/controller; owns mapping public handles to current private tokens, global operation serialization, authoritative completion and helper UI |
| File Manager media presenter | Public-client-only device row projection and mounted-directory navigation; existing local file authority still owns file operations |
| Native chooser media presenter | Same public client, request-scoped rows/navigation and result invalidation; portal resident/request policy and standard chooser wire remain unchanged |

Protocol and client are separate small targets with installed public headers.
Owner implementation does not become a service locator; dependencies remain
constructor-visible. Public values are copied immutable snapshots. QObject
client/transport/exporter are same GUI thread, dependencies outlive borrowers,
pending callbacks use object-context lifetime fences, and destruction withdraws
publication without emitting a late result. Client destruction does not cancel
an already accepted privileged operation. The manager approved this boundary after exact independent design review; add
the module-boundary registry rows with the first implementing targets.

### Bounded inventory and identities

A snapshot has `protocolVersion`, exact provider owner, opaque instance epoch,
monotonic revision, availability (`loading`, `ready`, `unavailable`), bounded
rows, one globally pending operation and a typed content-safe diagnostic.
Every publication is atomic. Limits: 32 drives, 128 visible volume rows,
256 KiB encoded snapshot, eight mount roots per volume, 4096 UTF-8 bytes per
absolute local mount root, 256 bytes per label/kind, 1024 bytes per explanatory
message and 64 ASCII bytes per opaque identifier. Invalid or oversized data
fails closed; it must not silently hide sibling partitions and admit removal.

Each row contains:

- opaque `driveDisplayId` and `volumeDisplayId`, bounded display name/kind,
  partition display number where available and unsigned size bytes;
- opaque attachment handle plus per-attachment generation, mount roots and
  preferred navigation root, mount state, read-only truth (`unknown`, `read-only`,
  `writable`), encrypted/locked and optical flags;
- owner-admitted `open`, `mount`, `mountReadOnly`, `unmount`, `remove` and
  `showDetails` availability with a typed reason for each disabled action;
- current progress phase (`idle`, `confirming`, `mounting`, `unmounting`,
  `locking`, `ejecting`, `poweringOff`, `refreshing`) and a typed outcome.

Display ids are stable across ordinary snapshots and mount changes within one
provider instance. The owner maps its private stable filesystem/drive identity
to opaque display ids; media without a stable identity gets an instance-local
id. No cross-login stable-id promise or saved consumer state is introduced.
Duplicate names remain separate rows; every eligible partition has its own
volume id while sibling volumes share a drive display id. An unlocked cleartext
child replaces the locked row without inheriting its action handle. Hidden
siblings remain private but are included by owner removal policy.

Display ids and mount roots are presentation/navigation data, never authority.
A handle is valid only with its exact owner, epoch and generation; unplug,
identity replacement, object removal or UDisks-owner replacement revokes it
immediately before debounce. Reused device/object/mount paths cannot resurrect
it. The owner epoch also advances when its underlying UDisks authority changes.
Provider loss clears rows and disables actions immediately; no stale ready
inventory is retained. Malformed publication does not resurrect previous rows.
The owner must extend its private typed projection for complete mount roots,
unknown read-only facts and operation phases/results at the point those facts
are observed; the public adapter must not infer them from human status strings
or optimistic helper selection. Local helper and public exporter consume that
same owner truth instead of creating another operation policy.

### Wire and asynchronous operations

The Devices object has `Version` (`u`, fixed 1), `GetSnapshot() -> ay`,
`RequestAction(ay) -> ay`, `SnapshotChanged(ay)` and `OperationFinished(ay)`.
All `ay` values use the protocol's canonical validated envelope. Specify a
little-endian versioned binary codec with a fixed magic/message-kind header,
fixed-width unsigned integers, length-prefixed UTF-8 strings and explicit
collection counts (following the existing Network1 codec pattern without
importing its network-specific types). Reject invalid UTF-8, unknown enums,
duplicate ids, noncanonical Boolean/length fields and trailing bytes before
publication; no arbitrary QVariant maps cross the wire. A snapshot
and operation result are at most the bounds above; request/result envelopes
are at most 16 KiB. The client uses exact unique-owner destinations; a
well-known name replacement cannot complete an old request.

A request carries a caller-generated opaque request id, observed owner/epoch/
revision, attachment handle/generation and a closed action enum: mount,
mount-read-only, selected-volume unmount, whole-drive remove, or show-details.
It carries no arbitrary path, UDisks option, force bit, credential or preference.
Refresh is read-only GetSnapshot and never replays a request. Admission returns
`accepted` with an operation id or a typed immediate refusal (`unsupported`,
`unavailable`, `stale`, `gone`, `busy`, `not-admitted`, `invalid`). The owner
rechecks current backend truth before issuing any privileged step. Owner-wide
serialization includes remembered mounts and local helper actions; clients
cannot bypass pending work by opening a second connection.

The owner retains at most 64 recent request ids/results for five minutes,
scoped to caller unique name and epoch. Duplicate ids return the previous
admission/result without issuing another mutation; a full bound refuses new
requests rather than evicting a still-live operation. This is bounded accidental
duplicate suppression, not a durable replay journal. Explicit later retry uses
a new id and fresh snapshot after the user inspects uncertain state.

Completion has operation id, initiating lineage, action and typed status
(`applied`, `refused`, `cancelled`, `busy`, `gone`, `uncertain`) with bounded
presentation text and optional confirming snapshot revision. No consumer
optimistically changes mount state or says safe to unplug on admission.
A mount navigation result becomes usable only after fresh same-attachment
readback confirms its absolute mount root; read-only success requires the
owner's existing verification. Unmount readback must confirm all selected
volume mount roots cleared. Readback timeout is uncertain, even after a
successful request reply; it never replays the write.

Whole-drive remove means all mounted siblings, including hidden ones, are
unmounted, unlocked backing devices locked, and the advertised Eject or
PowerOff operation completed by UDisks. For drives offering neither, confirmed
complete sibling unmount/Lock is an explicit `unmounted` removal mode, not an
invented power-off. Busy or cancelled authorization halts remaining steps;
partial progress and still-mounted siblings stay visible. Final authoritative
privileged completion is recorded before ordinary disappearance can retire the
row. Natural row disappearance alone, including hot-unplug, never proves safe
removal. A late result after owner/drive-generation replacement is uncertain
and cannot mark replacement media safe or navigate it. Consumers show **Safe
to unplug** only from the matching final success; readback/pending work remains
visible until that boundary. This adapts existing serialized operations; it
does not add force or a new destructive capability.

Show-details selects the current attachment in the owner's existing UI. Locked
encryption and formatting stay there; no passphrase crosses the public client.
The backend's authorization-cancel outcome is distinct from busy/refusal. Once
accepted, operations cannot be recalled by a chooser closing or consumer
client destruction; retire only that consumer's result/navigation interest.
No public Cancel mutation is promised because current UDisks calls cannot
establish reliable cancellation. Format confirmation remains captured-attachment,
explicitly destructive owner UI and is neither a sidebar action nor protocol enum.

### File Manager and chooser consumption

Devices appear as a separate live section alongside existing places/bookmarks;
consumer persistence remains unchanged. Rows show duplicate-name distinction,
mounted/read-only/encrypted status, progress and disabled reasons in accessible
text. Keyboard navigation, focus reveal and ordinary compact scrolling use the
consumer's own view. No media appearance policy leaks into the shared client.

Selecting a mounted row navigates its current preferred local root through
existing normalized directory navigation. Selecting an unmounted mountable row
issues one explicitly requested mount, retains progress and navigates only
when that request and fresh readback agree. Locked/unsupported rows offer
owner details rather than collecting secrets. Merely opening a browser or
chooser never mounts, changes preferences or opens the helper window.

Unmount/remove are explicit row actions. All partitions of a pending-removal
drive show pending state; no per-volume shortcut bypasses sibling work.
On unplug or owner loss, device-derived action/selection provenance is revoked,
late navigation/listing results cannot reopen its row, and removed-selection
text is visible. File Manager does not silently switch to a replacement path.
The chooser disables acceptance of a device-derived selection after its
attachment is lost until explicit current navigation/selection revalidates it.
Ordinary independently chosen local paths retain the existing filesystem
validation and portal request policy.

The chooser's standard OpenFile/SaveFile/SaveFiles result wire and parent/
frontend/Request lifetime remain unchanged. It never returns a device handle
to the sandbox. Known read-only roots remain browseable, with Save acceptance
disabled for that media selection; current filesystem checks still gate all
writes, and unknown writable truth does not grant write authority. Closing or
cancelling a chooser returns the existing cancellation result and cannot later
open a folder from an accepted mount callback. File Manager's file operations,
per-volume Trash and chooser save eligibility remain separate authorities.

## Verification and dispatch sequence

1. After this ADR's independent acceptance, dispatch the protocol/value codec,
   owner read-only exporter and public client as one inventory-only slice.
   Required focused fixtures: initial connected storage, multiple partitions/
   duplicate labels, ignored/dismissed notification while inventory remains,
   exact owner/epoch replacement, immediate hot-unplug/path reuse, malformed/
   oversized snapshots, unavailable UDisks and destruction during pending reads.
   Export no RequestAction until action admission/result gates exist. An old
   Activate-only owner must remain usable and report inventory unsupported.
   A recording launcher proves zero passive launches, one user Start attempt,
   observed-owner/readback readiness, launch refusal/timeout and visible Retry.
2. Review a separate owner/client ordinary-action slice: one-in-flight global
   admission, duplicate-id suppression, fresh mounted/read-only readback,
   stale generations, busy hidden sibling, authorization cancel, encrypted
   child Lock transition, delayed reply against reused drive and uncertainty
   without replay. Assert no format/passphrase/force option in public headers,
   wire or imported consumer dependencies. Installed consumer and withheld-
   module poison must load only the staged public modules.
3. Integrate File Manager sidebar using the public client and temporary local
   navigation roots. Prove mounted navigation, mount/readback deferred open,
   readonly/locked/duplicate rows, keyboard/focus/compact layout, removal progress,
   owner loss/hot-unplug and late-result suppression. No UDisks import or new
   choice persistence is allowed.
4. Integrate the native chooser presenter separately with the same client.
   Temporary paths and injected media fixtures prove every standard chooser
   method, known-read-only Save rejection, cancellation/frontend/parent loss,
   lost-device selection invalidation and zero automatic mount on opening.
5. Installed disposable-media qualification then observes insertion with
   notifications active, dismissed and unavailable; startup with media already
   connected; graphical mounted/unmounted/locked paths; busy removal; actual
   safe eject/power-off and unplug. Record filesystem/hardware coverage rather
   than claiming universal USB behavior. Destructive formatting remains its
   own explicitly authorized disposable-media test, outside these packets.

Existing owner regressions to preserve are `qindaqt.removable-media-policy`,
`qindaqt.removable-media-udisks`, `qindaqt.removable-media-notifications` and
`qindaqt.removable-media-qml`. Private-bus rows require the manager's runtime
lease. New client/value rows must be host-independent; physical devices and
live preferences never satisfy an automated fixture.

## Consequences and open implementation boundary

Consumers gain one discoverable inventory without moving storage authority or
remembered choices. The additional session-bus ordinary-action surface is a
new process contract and requires exact-owner, bounded codec and no-replay
review. Implementations must keep public protocol/client small and owning
private operations private. This document is Proposed, adds no feature-ledger
progress and authorizes no product code before review. The first implementation
packet is the observed inventory/public-client slice; sidebar delivery follows
its accepted candidate, not a second device watcher.

# Removable-media client and Devices exporter

[ADR-0350](../adr/0350-share-removable-media-with-file-consumers.md) keeps inventory, insertion choices and UDisks authority inside the session-owned [Removable Media application](../apps/removable-media.md). The public [Devices-v1 values/codec](../reference/removable-media-protocol-v1.md) and `QindaQt::RemovableMediaClient` give consumers owning snapshots without importing the application's private backend, preferences or UDisks paths.

## Inventory and recovery

`MediaSource` is the same-thread constructor-injected presentation seam. `MediaClient` borrows an existing registered session connection and a `MediaOwnerLauncher`; both outlive it. The optional injected `MediaOwnerLookup` is an observational port for the fixed service; its borrowed implementation also outlives the client. It returns copied immutable snapshots. `start` and `refresh` observe an existing owner with asynchronous five-second reads. They never activate the helper, launch mount commands or run its insertion policy. Owner lookup and snapshot reads are independently single-flight, including repeated Refresh. A failed/deadline lookup remains retryable, and an empty lookup cannot overwrite Loading while deliberate startup is pending. Each call uses the observed unique owner and captured serial; owner replacement immediately clears rows and fences old replies/signals.

The owner privately exports Version=1, GetSnapshot and SnapshotChanged at `/org/qindaqt/RemovableMedia1/Devices`. Its adapter receives the existing backend and controller. Complete mount roots, partition numbers and known/unknown read-only truth originate in the owning projection; the adapter does not infer facts from status strings. It rolls an instance epoch on underlying UDisks authority replacement and salts public drive/volume/attachment identifiers. Duplicate labels and sibling partition rows stay separate. Bad roots, excessive inventory or invalid data withdraw the entire snapshot rather than hiding siblings. Dismissing a notification cannot withdraw this inventory.

A missing owner reports **Media support unavailable**. A deliberate `recover` requests one closed owner launch; launcher success remains Loading until an observed owner publishes valid ready inventory. Failure or five-second startup timeout stays visible and offers Try again/the ordinary launcher. Passive reads never request a launch and repeated clicks while starting do not loop. An old Activate-only owner reports Unsupported; deliberate recovery opens its existing window rather than installing a second watcher or compatibility mount utility.

All decoded publications are validated atomically. Malformed data clears ready rows. Same-epoch revision regression and reuse of a retired epoch are refused; the client keeps bounded epoch history separately from unavailable presentation. Raw broker/service errors and private identities are never shown.

## Ordinary actions and graphical launch

The owning exporter adds only the closed Devices-v1 RequestAction and
OperationFinished boundary. A caller sends the current owner/epoch/revision
and exact attachment, receives typed admission, and correlates a terminal result
to that admitted operation. Admission rechecks current private inventory and
owner-wide serialization across insertion choices, owner UI and consumers.
A 64-entry, five-minute monotonic caller/epoch/request cache retains accepted
work and terminal results; duplicate identical requests return that admission
without dispatching again. Changed payloads under the same request id are
invalid. Exhaustion returns Busy rather than evicting pending work. Formatting,
passphrases, preferences, force flags and arbitrary UDisks paths never cross
this interface.

The private UDisks adapter confirms the captured attachment before dispatch.
Mount and unmount success require fresh complete mount-root facts; read-only
mounts also require known read-only truth. Safe removal covers hidden siblings
and cleartext backing volumes, reads back every sibling's released/locked state
before final Eject/PowerOff, and accepts only the captured drive's successful
final callback. An announced drive or sibling/selected block replacement immediately fences that reply, even
before inventory debounce. Drives with neither final method require observed
complete unmount/lock convergence. Partial, cancelled, busy and uncertain
outcomes carry no safe-unplug claim and never replay.

The client admits one local request at a time, uses its observed unique owner
without D-Bus autostart, and bounds admission/readback to five seconds and
accepted completion to two minutes. It buffers one early terminal result until
the admission authenticates its operation id. Applied mount/unmount remains
pending until a current validated snapshot reaches the confirming revision
with matching attachment and mount/read-only state. Owner/epoch loss, revoked
attachment, malformed reply, contradictory readback and deadline withdraw
interest with a typed failure. Refresh and destruction neither cancel accepted
owner work nor replay it.

The optional QindaQt::RemovableMediaLaunch target is separate from the Qt
Core/DBus-only client. DesktopMediaOwnerLauncher owns injected XDG data roots,
resolves only org.qindaqt.RemovableMedia.desktop through public
ApplicationCatalog scanning and launch planning, and passes the validated
literal argv to a same-thread injected MediaArgvStarter. Its default uses
QProcess. Construction is inert. Missing/deleted/unsupported/terminal-only/
D-Bus-only entries return failure with no fallback; only deliberate startOwner
may submit a process. ApplicationCatalog's suffix-free id is derived from that
fixed installed filename.

## File consumers and qualification

[File Manager](../apps/file-manager.md#removable-devices-and-location-lifetime)
and the [native chooser](../reference/portal-choosers.md#removable-devices)
borrow MediaSource. Their source is independently owned presentation and
selection/navigation interest; neither imports private backend or preferences.
Consumers bind location provenance to the most-specific unique mounted root.
File Manager observes controller creation as well as active-tab changes, so a
never-active Commander pane is covered. The chooser rechecks its captured
selection/navigation generation and current media after nested overwrite
confirmation and immediately before publication. No observed row triggers mounting. Deliberate open of an unmounted row waits
for the admitted action's confirmed current root; closing/changing the target
withdraws only deferred navigation.

The standalone entry point now includes qindaqt.removable-media-actions and
expanded owning UDisks failures beside inventory/policy/notifications.
Focused consumers are qindaqt.file-manager-media, qindaqt.file-manager-media-ui and
qindaqt.portal-media-chooser; owner launch has
qindaqt.removable-media-owner-launcher. They use temporary filesystem fixtures,
public source doubles and private/absent buses. Compiler, runtime, native
frontend, installed-package and physical USB evidence must each name the exact
tested candidate. Source fixtures never establish physical-media qualification.

Source candidate 64dcfa1a2b15c065c97d43e0f30e5c6bca173633 passed the strict focused
production build and all 29 selected registry rows: 339 Qt checks, zero failures,
skips or blacklisted cases. This includes actual portal frontend/private-KWin
native chooser mapping and lifetime checks, not only source doubles. The
Core/DBus installed-only staged consumer passed 1/1; withholding its staged
client header broke compilation and restoration passed 1/1. These results
qualify that candidate's source and private fixtures. Integration rechecks,
Portage payload/session checks and physical USB insertion/mutation/eject remain
separate delivery gates.

The isolated `tests/services/removable_media_client/standalone` entry point links the public protocol/client plus owner-only projection/controller fixtures. `qindaqt.removable-media-inventory` uses a private session bus and unavailable system bus. It covers passive zero-launch, explicit start/readback/timeout, old-owner Activate compatibility, malformed/oversized snapshots, epoch retirement, immediate owner revocation, destruction during pending read, duplicate partitions, complete roots and unknown read-only truth. Execution evidence is recorded with the exact source candidate handoff; it is private-bus verification, never physical-media qualification.

The sibling `installed_consumer` CMake entry point takes `MEDIA_STAGE` and links
only the staged client/protocol headers and archives plus Qt Core/DBus. A
withheld staged client header must fail compilation, and a consumer with both
buses absent must publish unavailable without requesting a helper launch.

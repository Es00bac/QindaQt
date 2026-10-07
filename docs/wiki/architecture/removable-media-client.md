# Removable-media client and Devices exporter

[ADR-0350](../adr/0350-share-removable-media-with-file-consumers.md) keeps inventory, insertion choices and UDisks authority inside the session-owned [Removable Media application](../apps/removable-media.md). The public [Devices-v1 values/codec](../reference/removable-media-protocol-v1.md) and `QindaQt::RemovableMediaClient` give consumers owning snapshots without importing the application's private backend, preferences or UDisks paths.

## Inventory and recovery

`MediaSource` is the same-thread constructor-injected presentation seam. `MediaClient` borrows an existing registered session connection and a `MediaOwnerLauncher`; both outlive it. It returns copied immutable snapshots. `start` and `refresh` observe an existing owner with asynchronous five-second reads. They never activate the helper, launch mount commands or run its insertion policy. Each call uses the observed unique owner and captured serial; owner replacement immediately clears rows and fences old replies/signals.

The owner privately exports Version=1, GetSnapshot and SnapshotChanged at `/org/qindaqt/RemovableMedia1/Devices`. Its adapter receives the existing backend and controller. Complete mount roots, partition numbers and known/unknown read-only truth originate in the owning projection; the adapter does not infer facts from status strings. It rolls an instance epoch on underlying UDisks authority replacement and salts public drive/volume/attachment identifiers. Duplicate labels and sibling partition rows stay separate. Bad roots, excessive inventory or invalid data withdraw the entire snapshot rather than hiding siblings. Dismissing a notification cannot withdraw this inventory.

A missing owner reports **Media support unavailable**. A deliberate `recover` requests one closed owner launch; launcher success remains Loading until an observed owner publishes valid ready inventory. Failure or five-second startup timeout stays visible and offers Try again/the ordinary launcher. Passive reads never request a launch and repeated clicks while starting do not loop. An old Activate-only owner reports Unsupported; deliberate recovery opens its existing window rather than installing a second watcher or compatibility mount utility.

All decoded publications are validated atomically. Malformed data clears ready rows. Same-epoch revision regression and reuse of a retired epoch are refused; the client keeps bounded epoch history separately from unavailable presentation. Raw broker/service errors and private identities are never shown.

## Current slice and next boundary

This inventory slice has no RequestAction export. It does not deliver File Manager or chooser device presentation, ordinary operation admission, or physical USB qualification. Ordinary actions must next retain owner-wide serialization, exact attachment admission, duplicate suppression, no replay and authoritative convergence before consumer navigation or Safe to unplug. Credentials, formatting and saved choices remain exclusively in the owner UI.

The isolated `tests/services/removable_media_client/standalone` entry point links the public protocol/client plus owner-only projection/controller fixtures. `qindaqt.removable-media-inventory` uses a private session bus and unavailable system bus. It covers passive zero-launch, explicit start/readback/timeout, old-owner Activate compatibility, malformed/oversized snapshots, epoch retirement, immediate owner revocation, destruction during pending read, duplicate partitions, complete roots and unknown read-only truth. Execution evidence is recorded with the exact source candidate handoff; it is private-bus verification, never physical-media qualification.

The sibling `installed_consumer` CMake entry point takes `MEDIA_STAGE` and links
only the staged client/protocol headers and archives plus Qt Core/DBus. A
withheld staged client header must fail compilation, and a consumer with both
buses absent must publish unavailable without requesting a helper launch.

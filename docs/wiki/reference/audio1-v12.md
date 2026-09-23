# Audio1 schema 12: manual VBAN peer definitions

This page describes the schema-12 additions to the
[Audio1 version-2 base contract](audio1-v2.md). The bus name, object path,
owner/epoch/revision rules, bounded D-Bus transport, and prior operations
remain as documented there.
Both computers need schema-12 Audio1 service and client builds for the typed
peer controls and link-based `active` readback. An older service has no typed
CRUD methods, and an older client rejects the newer snapshot schema; mixed
versions fail closed rather than silently applying only part of a peer setup.
The private two-host test runs the candidate binaries on both hosts, not the
installed Audio1 services.


`Snapshot.schemaVersion` is 12. `VbanStream` in `Console.vban` has the
D-Bus tuple `(sbssubbs)`: `name` (string), `outgoing` (bool), `busId`
(string), `host` (string), `port` (uint32), `enabled` (bool),
`active` (bool), `outputNodeName` (string). The final field is empty
outgoing and names the exact physical output incoming. Incoming `host`
is the exact canonical IPv4 source authorized by the user; outgoing
`host` is the destination. `busId` remains outgoing-only.

`ManageVbanStreams` is capability bit 10. `UpsertVbanStream` (operation
kind 28) carries a validated `vbanDefinition` value with `enabled=false`
and `active=false`; `DeleteVbanStream` (kind 29) names a saved stream.
Both return the ordinary typed `OperationResult`. For these name-based peer
controls, a same-owner reply in the same epoch may begin at a newer revision
than the client's last snapshot; the client accepts only non-regressing initiating
and observed revisions. Handle-targeted operations retain exact initiating-revision
checks. An upsert atomically persists the definition and disables its existing switch. `SetVbanEnabled`
(kind 27) separately authorizes local activation and refuses unknown names.
The client and service both reject malformed definitions; legacy incoming
definitions with no source/output cannot be activated.

`enabled` means the user has requested activation. `active` means
Audio1 observed connected PipeWire links on the selected local graph path
(Paused or Active link state). A sender needs the exact bus target-to-capture
edge; a receiver needs both the VBAN source-to-loopback capture edge and
the loopback playback-to-selected physical output edge. A changed or re-enabled
route remains inactive until link evidence for its fresh internal activation
token arrives; delayed evidence from the previous route is ignored. The token
is backend-internal and does not change the schema-12 D-Bus tuple. Neither
value confirms remote delivery, authenticity, or audible physical speaker
output. See
[Audio VBAN streams](audio-vban-streams.md) and
[ADR-0246](../adr/0246-configure-manual-audio-peers-through-audio1.md).

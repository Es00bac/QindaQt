# ADR-0246: Configure manual audio peers through Audio1

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Platform (audio service) and First-party (Settings Audio)
- **Supersedes:** [ADR-0185](0185-vban-is-a-document-and-two-threads.md) for stream definitions, receive admission, and Settings control
- **Superseded by:** None

## Context

ADR-0185 made VBAN transport available, but people still had to hand-edit a
JSON document. Its receiver listened for any sender with the stream name and
port, and merely surfaced a virtual source; choosing which local speakers
should play that source was left to another routing step. A Settings switch
could report a stream active when the graph path was absent. That did not
provide a usable two-computer setup from Settings.

## Decision

Audio1 schema 12 adds typed `UpsertVbanStream` and `DeleteVbanStream`
operations and the `ManageVbanStreams` capability. `VbanStream` appends an
incoming-only `outputNodeName`; the existing outgoing `busId` keeps its
sender meaning. The service validates identities, port and direction-specific
fields, persists the definition document atomically, and republishes
authoritative state. Saving any definition returns its enable switch to off,
so editing a live receiver cannot silently change the permitted host or
speaker. The existing `SetVbanEnabled` operation is the deliberate
activation step. Since these three peer controls address stream names
rather than graph handles, a service reply may begin at a newer snapshot
revision than the client's last read. The Audio1 client accepts only same-owner,
same-epoch, non-regressing reply revisions for them; handle-targeted operations
keep exact revision fences. A receiver definition requires one exact canonical
IPv4 source address, one UDP port, and one physical output node name. The receive
thread checks the actual datagram source before decoding packets. Duplicate
receive ports are refused, since concurrent sockets cannot reliably share
their VBAN traffic.

An enabled receiver creates a dedicated PipeWire loopback from its VBAN
source to the selected physical output. Both loopback sides specify exact
`target.object` names and `node.dont-fallback`; absence of the selected
output withdraws the route rather than selecting another speaker. The
console's unrelated strip and bus routing is not changed. The sender and both
receiver halves also pin their targets against WirePlumber's default-target
metadata rewriting. `active` now means the selected local capture or
receive-to-speaker **links** were observed in PipeWire at a connected
(Paused or Active) state, not merely that their nodes were registered. It
does not claim remote packet delivery or audible playback.

Settings Audio has Devices, Mixer, and Other computers tabs. The peer tab
offers manual host/port entry, exact receiver authorization, physical speaker
selection, saved stream editing/removal, and separate Enable controls. It
states that VBAN is open LAN UDP without encryption or peer authentication.
This is a manual-peer slice; discovery, pairing, encrypted transport,
remote audibility confirmation, and synchronized multichannel playback
require later decisions.

## Consequences

- Old outgoing JSON definitions remain readable. Legacy incoming definitions
  without `sourceHost` and `outputNodeName` are ignored and cannot open a
  wildcard listener.
- Both endpoints need schema-12 Audio1 clients and services for typed peer
  controls and link-based `active` truth. Schema-11 clients reject schema 12;
  schema-11 services lack the CRUD methods. The `outputNodeName` tuple field
  is a public wire change. The private two-host proof uses candidate binaries
  on both machines, not installed services.
- Configuration can be completed at both ends from Settings. A successful
  local route still needs a reachable peer, network permission, and working
  physical audio output to produce sound.
- Service tests must cover admission by datagram source, atomic definitions,
  unavailable-output no-fallback behavior, and private graph routing. Settings
  tests must cover tab navigation and compact focus.

# ADR-0324: Separate native portal capture authority, consent and stream lifetime

- Status: Proposed; source and actual frontend/PipeWire qualification pending
- Date: 2026-10-01
- Amends: [ADR-0318](0318-native-portal-foundation.md), [ADR-0289](0289-native-screenshot-and-record-tool.md)

## Context

Screenshot and ScreenCast need actual compositor pixels and long-lived streams,
while the resident portal must remain a GUI-free composition of public ports.
The Screenshot application's private capture implementation cannot become a
cross-module portal dependency. Native lock uncertainty must suppress publication
and withdraw retained results and streams.

## Decision

Extract shared request building, bounded raw decoder, region geometry and the
restricted screenshot transport into a public CompositorCapture module. Retain
thin Screenshot compatibility headers. Add an independently owned supplied-native-FD
Wayland ScreenCast port; the generated installed primary protocol is the only
compositor protocol dependency, never fork-private headers.

Portal pure wire policy, actor-fenced standard Requests/Sessions, child-process
lifetime and native Qt dialogs remain separate components. The protected source
successor uses a compositor-launched capture-only broker rather than granting
the general ADR0318 resident ambient capture permission. Frozen fork wire
91e1c202, with configured-path correctionbd4eacd1, defines fixed broker/helper images and private authenticated control;
consumers include its installed Qt-free public header only. The general resident
and native Session1 attachment remain supervisor-owned. This interface agreement
is not code/runtime acceptance; both families remain routed to KDE pending gates. The helper retains its legacy Screenshot2 compatibility desktop entry; the
capture broker has no ambient restricted-global desktop permission or activation. Selected
ordinary peer/PIDFD/current native owner and authenticated read-only lock receipts
gate consent, operation and publication. Native authority loss withdraws pending
results, deletes temporary screenshot files and ends streams. Already copied
image bytes or consumed stream buffers cannot be recalled.

The capture-only backend's public availability follows authenticated native
initialization. Its own retained-peer/current-owner monitor must consume both
the targeted nonce state receipt and empty request reply before publishing the
existing backend name/object and starting its QCC1 channel. Known Locked/Locking
state initializes the backend but denies content; startup is not pixel authority.
The broker name still precedes Ready so the compositor can authenticate its
current owner. No public wire bytes, descriptors, helper consent sequencing or
request-time privacy rechecks change. Initialization and Hello share the original
five-second startup bound; owner loss cannot reuse an earlier initialized state.

The real standard frontend owns OpenPipeWireRemote and node-specific permission
filtering. Backend Start returns only a real created node. No fabricated remote,
success placeholder, hidden auto-selection or ambient display fallback is allowed.
The fork's existing ScreenShot2 and KDE owner compatibility names remain untilPF21.

The first reviewable slice supports Screenshot/PickColor and ScreenCast version2
monitor selection with one source and Hidden cursor. Unsupported types, multiple
selection, cursor modes and restore/persist inputs fail explicitly. This does not
complete PF19: window/multiple/cursor capabilities and shared revocable restore
permissions remain a separately scoped successor before program closure. Metadata
and routing change only after actual native frontend/PipeWire/privacy gates.

## Consequences

Policy and resident lifetime components have no GUI/PipeWire dependency. Native
helpers own presentation and actual capture over separate ordinary GUI and
tagged capture connections; private test input links unchanged
GUI sources. Installed desktop/sandbox delivery remains manager-owned and cannot
be inferred from temporary private frontend and compositor qualification.

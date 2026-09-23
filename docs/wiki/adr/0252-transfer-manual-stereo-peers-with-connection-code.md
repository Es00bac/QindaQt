# ADR-0252: Transfer manual stereo peer settings with a connection code

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** First-party Settings Audio
- **Extends:** [ADR-0246](0246-configure-manual-audio-peers-through-audio1.md) for the Settings setup flow

## Context

The schema-12 Audio1 service can send and receive stereo between two computers,
but both people must manually copy a matching name and port and know the
sender's address. That is error-prone even though the service's explicit
receive authorization and physical-output route are sound.

## Decision

Settings Audio can encode one **saved outgoing** stream's name and UDP port
with an explicitly selected local IPv4 source address in a versioned
`QINDAQT-AUDIO-1:` code. Settings lists active non-loopback IPv4 addresses
using QtNetwork, lets the user refresh and copy an address for the other
computer, and permits manual override for multihomed or VPN use. The
transfer format is canonical base64url of a bounded JSON object with exactly
`version`, `name`, `sourceIpv4`, and `port`. Decode rejects malformed,
foreign-version, oversized, noncanonical, and Audio1-invalid values. The
format carries no destination, credential, permission, device identity, or
claim of peer authentication.

A generated code is tied in Settings to the exact Audio1 owner and epoch
and saved outgoing name, bus, destination host, and port. A snapshot refresh
that preserves those fields keeps the code copyable. A changed or removed
sender or replaced service revokes it, even when the code's transfer tuple
would still decode. This identity is local UI state and never enters the
transfer format.

The receiving Settings page reviews the tuple against the current Audio1
snapshot and refuses an existing name or occupied receive port. The user
chooses a current exact physical output and acknowledges the trusted-network
warning. Save revalidates the code, conflict, and output against the current
snapshot and sends the existing typed Audio1 upsert. The new receiver remains
disabled. The existing explicit Enable action is the only way to admit
packets. Successful dispatch is labeled as a request until Audio1 readback
publishes the row. Manual send/receive forms remain available.

No Audio1 schema, stream persistence, source-IP admission, or no-fallback
graph rule changes. Both machines still require schema-12 Audio1. A code
does not discover the other computer, encrypt traffic, authenticate a sender,
confirm remote playback, or synchronize surround audio.

## Consequences

- A setup in each direction uses a separate saved outgoing stream and code.
  The recipient chooses its own speakers and explicitly enables reception.
- Network address selection is a convenience; routing across firewalls,
  multiple interfaces, or VPNs still requires a reachable chosen address.
- Settings tests cover code round trip, malformed/foreign values, live
  conflicts, and compact/wide page focus. Audio1's existing private graph
  tests remain the authority for source admission and exact speaker routing.

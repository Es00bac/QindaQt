# ADR-0310: Native per-application Secret portal

- Status: Accepted
- Date: 2026-09-30
- Supersedes: Secret routing row in [ADR-0133](0133-route-every-portal-family.md)
- Related: [ADR-0296](0296-native-keyring-daemon-boundary.md), [ADR-0302](0302-native-passwords-keys-client-and-policy.md)

## Context

Removing Plasma dependencies requires the native key store to serve the
standard Secret portal. Unrelated item-reveal authority cannot authorize an
application secret. Existing encrypted app data also depends on exact legacy
portal bytes. Ordinary QtDBus method replies do not authenticate their sender.

## Decision

A separate pure policy and platform transport module composes into the existing
resident without changing appearance behavior. Only the current authenticated
frontend supplies case-sensitive application identity; empty host IDs fail.
The owning daemon selects PK3 login by fixed ID and persists a dedicated32-byte
fresh-native-32 record after existing create/unlock prompts. Metadata/hash/length
collisions fail explicitly. Exact native Unlocked admission is required even
when collection screen-lock policy is false. Targeted fresh-nonce receipts
with actual sender/signature/current owner prove policy and secret results;
RPC success alone never grants authority. Native UI policy uses the same receipt.

Requests, prompts, rates, identities, receipt tombstones and FD writes are
bounded. Owner/privacy/cancel loss clears owned plaintext and no action replays.
The standard Secret Service protocol and existing password authority stay
compatible. Settings and Secret are the only advertised native portal families;
only Secret's routing row changes to qindaqt. Public composition exposes a
borrowed backend QObject without leaking appearance transport internals.

## Consequences

Host Registry IDs retain weaker host security semantics. Pipe nonblocking flags
are shared with the caller; delivered bytes and framework copies cannot be
recalled. Fresh32-byte records do not migrate KWallet's existing opaque64 bytes.
PK6 must preserve exact legacy bytes/identity/provenance through a versioned
atomic conflict-aware import before live replacement. No installation or real
wallet access is authorized by source qualification.

The full primary contracts, limits, persistence and verification matrix are in
[native Secret portal](../architecture/secret-portal.md). The preceding routing
ADR remains historical; other family rows and appearance behavior are unchanged.

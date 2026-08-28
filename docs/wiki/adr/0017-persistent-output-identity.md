# ADR-0017: Derive privacy-preserving persistent output identities

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** Display identity and Settings schema modules
- **Supersedes:** None
- **Superseded by:** None

## Context

Connector names change across docks and ports. KWin's compositor output UUID is
a runtime protocol handle that may also change across hotplug events. Neither
is a sufficient persistent key for aliases, labels, or per-output policy.
Conversely, publishing a monitor serial or raw EDID would expose private
hardware material to every snapshot consumer.

QindaQt should identify the same physical monitor consistently with the
matching precedence used by pinned KWin 6.6.5's
`OutputConfigurationStore::findOutputIndex`, while refusing to guess when
connected hardware is indistinguishable. The module and public schema are
specified in [Display1 version 1](../reference/display1-v1.md).

## Decision

For one bounded connected-output batch, derive each stable identity using the
first unique material in this order:

1. SHA-256-truncated EDID identifier material;
2. SHA-256-truncated complete raw EDID;
3. SHA-256-truncated composite of valid EDID material and MST path; then
4. a bounded safe connector name, or its SHA-256-truncated hash when the name
   cannot appear directly.

Hashes expose 16 bytes as lowercase hexadecimal with a source prefix. Raw
EDID, serial text, and un-hashed private hardware material never enter resolved
values, registry values, diagnostics, or protocol snapshots. Manufacturer,
model, physical size, `hasSerial`, and `internal` are bounded descriptive
metadata only.

Duplicate stronger material is explicit `ambiguous=true`; the resolver falls
through to the next discriminator instead of conflating outputs. A residual
digest/fallback collision receives deterministic `#n` suffixes in connected
output order and is also ambiguous. An ambiguous identity cannot receive an
alias. Malformed EDID is never hashed as trusted material and falls back to MST
path or connector rules.

The compositor UUID is validated as hostile input but is absent from the
resolved output and registry. Connector rename can update `lastConnector`
without changing a previously accepted EDID-based stable ID. A same-connector
EDID replacement yields a new identity; the old bounded registry entry remains
until least-recently-seen eviction.

D1 owns a pure schema-v2 registry value and v1-to-v2 migration. It neither
reads nor writes Settings1. A later Settings schema-v3 migration owns the
bounded persisted policy/registry key; it must consume this public value
contract rather than duplicating identity logic.

## Consequences

- Stable IDs are deterministic for one connected inventory and suitable for
  aliases without leaking serial or raw EDID material.
- Truly indistinguishable displays remain separately addressable by bounded
  fallback identities but are visibly ambiguous and cannot silently inherit an
  alias.
- The registry is capped at 64 entries and 32 aliases. Reconciliation refreshes
  a caller-supplied monotonic `seenSequence` and evicts the oldest disconnected
  entry first, with stable-ID tie breaking.
- Moving a monitor with valid unique EDID material preserves identity; a
  connector-only monitor cannot be proven identical after a connector rename
  and intentionally receives a different fallback identity.
- KWin matching behavior remains a compatibility input. The public hash
  representation and registry schema are QindaQt contracts and require an
  explicit version change if altered.

## Revisit when

Reconsider when pinned KWin changes matching precedence, a public compositor
identity is proven persistent across duplicate and hotplug cases, or privacy
requirements demand keyed/local-only identifiers. Migrate existing registry
values explicitly; never silently reinterpret a stable-ID prefix.

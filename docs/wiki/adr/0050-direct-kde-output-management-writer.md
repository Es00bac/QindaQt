# ADR-0050: Use a direct bounded KDE output-management writer

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Display platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0016](0016-display1-transaction-authority.md) chooses direct KDE public
output-management as the production mutation path and forbids a competing
KWin restore store. The D2 inventory currently exposes only connector-fallback
stable IDs and one synthesized current-mode identity. D4 needs a real protocol
boundary without pretending those identities can select arbitrary advertised
modes or EDID-derived outputs.

Using libkscreen in production would add a second policy and cache layer around
the public protocol. Using `kscreen-doctor` would add a process/text boundary
whose ordering and result identity do not satisfy Display1's exact token and
owner fences. KWin private headers or configuration files would violate the
accepted authority split.

## Decision

Implement D4 as a direct Qt-generated client for exact, vendored KDE
output-device-v2 and output-management-v2 XML. Keep all generated wrappers and
Wayland objects private behind an injected `OutputManagementPort`; expose only
bounded owning values, observer callbacks, and a production factory.

Accept only `conn:` identities and exact `current:WIDTHxHEIGHT@MILLIHERTZ`
modes until the inventory publishes a stronger end-to-end identity/mode
contract. Require management protocol version 13 or newer for the full D1
topology semantics. Fail closed on ambiguity, unsupported identity or mode,
malformed topology, owner/global replacement, transport loss, timeout, or late
reply. Serialize exactly one apply and fence it by machine lineage, Display1
token, writer request ID, and owner generation.

Keep libkscreen and `kscreen-doctor` available only as independent test/oracle
inputs. Do not read KWin private state. Do not wire the production Display1
executable until durable journal and remaining session/recovery dependencies
can preserve the complete transaction contract.

## Consequences

- D4 has a small auditable mutation vocabulary and no KWin private ABI.
- The protocol XML and checksums become reviewed source inputs; compatibility
  changes are explicit rather than silently inherited from a runtime package.
- Connector rename and unsupported/opaque mode identities reject instead of
  selecting a plausible but different output or mode.
- Complete applies require the exact known device set. Surviving rollback may
  change only mode, scale, and transform on currently enabled outputs.
- Owner replacement, hotplug during apply, timeout, and connection failure are
  uncertain outcomes and never permission to replay a forward request.
- Deterministic fake tests and compiled protocol code do not qualify KWin
  convergence. A contained nested protocol row remains mandatory before the
  packaged service may claim operational writer authority.

## Revisit when

Revisit the identity/mode restriction when the authoritative inventory carries
stable EDID-backed identity and the full advertised mode set through Display1.
Revisit the direct client only if measured contained nested evidence proves the
public protocol cannot satisfy the transaction contract; library convenience
or UI demand alone is insufficient.

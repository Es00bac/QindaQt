# ADR-0030: Keep display color as a pure bounded model first

- **Status:** Proposed
- **Date:** 2026-08-28
- **Owners:** Display Color lane (Platform services workgroup)
- **Supersedes:** None
- **Superseded by:** None

## Context

QQ-005.07 needs color management truth before any platform lane exists: ICC
profile discovery/import, per-output assignment, and HDR/WCG policy all
eventually cross hostile boundaries (imported files, IPC, persistent
storage). Display1 already established the repo pattern — pure bounded value
modules first, transport and mutation later — and the orphaned Solene Ward
candidate had already shaped such a model. The recovery owner had to decide
whether to finish that pure slice or reach for direct platform integration
(ICC file enumeration, compositor color-management protocols) in the same
change. The owning architecture page is
[Display color model](../architecture/display-color-model.md).

## Decision

Display Color C0 is a pure value model with strict fail-closed bounds and no
platform authority. Specifically: ICC material enters only as injected
descriptor values with a bounded header validation (no file or host-profile
access); stable output IDs are validated by bounded opaque format without
linking Display1 identity code; the catalog is deterministically ordered;
assignment intent stays separate from applied truth with explicit degraded
reasons and sRGB/last-known-good fallbacks; snapshots are atomic and
fingerprinted with exact epoch/revision lineage equality. Enum ranges,
finite luminance, aggregate caps (32 outputs, 256 profiles, 4 MiB profiles),
and atomic reject-on-invalid are contract, not implementation detail.

## Consequences

- Later import, service, and compositor lanes consume validated values and
  must not loosen the bounds or decode around them.
- The module may not grow transport, persistence, or hardware access; the
  source-policy test row plus poison-negative proof guard this in CI.
- Checksum verification of full profile bodies is deferred to the import
  lane that actually holds the bytes.
- Truthful degraded states (HDR-on-SDR, missing profile, invalid profile)
  are pinned by tests so a UI lane can expose them without re-deriving
  policy.
- Any new dependency direction from this module requires a new or amended
  ADR.

## Revisit when

A color-management platform lane (ICC import daemon, compositor color
protocol, or Settings persistence) is accepted and needs this model to
change shape; or the ICC validation bounds demonstrably reject real-world
profiles that the import lane must accept.

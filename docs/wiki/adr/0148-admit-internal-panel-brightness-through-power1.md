# ADR-0148: Admit internal-panel brightness through Power1

- **Status:** Accepted
- **Date:** 2026-09-12
- **Owners:** Power platform service and the Power Settings route
- **Supersedes:** ADR-0060's exclusion of a Power1 display-brightness method
- **Superseded by:** [ADR-0186](0186-write-internal-brightness-through-logind.md), for the sysfs-only write rule and the read-only refusal state

## Context

[ADR-0024](0024-route-brightness-through-power1.md) placed internal-panel
brightness inside Power1. [ADR-0060](0060-confine-production-power-upstreams.md)
replaced its logind write with a narrow, permission-gated sysfs write
primitive, but kept Power1 version 1 without a display-brightness method. The
[Power Settings route](../apps/power-settings.md) therefore showed the built-in
display read-only.

Users need that row to be a real control. It must not weaken Power1's
exact-owner lineage or no-replay rule, and it must still refuse honestly when a
panel is read-only, ambiguous, or unavailable. Power1 cannot observe connector
topology: the KWin provider of ADR-0024 is still unbuilt.

## Decision

Power1 version 1 gains one additive operation:
`SetInternalBrightness((ts) device, u value) -> (uuttttss)`, operation kind `4`.
The snapshot, its D-Bus signature, and the canonical codec layout are
unchanged. The protocol version stays `1`: the change only extends the closed
operation vocabulary, and only this package's clients originate the new kind.

One pure `power_protocol` rule picks the target. The coordinator's request
validation, the production apply step, `PowerClient` preflight, and Settings
admission all call it:

- candidates are published internal backlights with a usable maximum;
- the kernel type preference firmware > platform > raw selects one tier;
- more than one candidate in that tier is ambiguous, and every one of them is
  refused;
- candidates in lower tiers are not selected;
- the selected device is admitted only while it is `Ok` with an observed value;
- a read-only or unreadable selected device never falls back to another device.

The production battery collaborator applies each admitted request on a later
event-loop turn, one at a time in submission order. It applies only while the
battery domain is published, and it re-checks the rule against current
inventory first. The write uses the ADR-0060 primitive, which re-reads
`actual_brightness` (or `brightness`) and publishes that observation before
the operation result. The result's observed revision therefore already
carries the kernel's answer. A denied write completes `Failed` with
`backlight-read-only`; nothing escalates privileges or retries.

Settings shows the selected panel as a keyboard- and pointer-operable slider.
It uses the same exact owner/epoch/revision fence, debounce, single pending
operation, and no-replay handling as keyboard brightness. The route converges
on the observed value. If the observed value differs, the wait ends at once
and that value is shown.

## Consequences

- The built-in panel can be adjusted in Settings wherever the resident service
  may write the injected sysfs root.
- Read-only, ambiguous, lower-preference, degraded, and unavailable panels stay
  visible with explicit text and no dispatchable control.
- Every Power1 battery-seam implementation must provide the new operation and
  complete it asynchronously. The D-Bus object records its delayed reply only
  after submission returns.
- Introspection now advertises handles as `(ts)`, matching their marshalling.
- The installed user unit sets `ProtectKernelTunables=true`, which mounts
  `/sys` read-only. A packaged service therefore publishes the panel as
  read-only, and Settings refuses honestly. That stays true until a separate
  hardening decision grants write access to the backlight root; ordinary file
  permissions can have the same effect. That is the supported refusal state,
  not a reason for a helper.
- PowerDevil keeps its session brightness-key ownership, and an external change
  is observed through readback rather than arbitrated.
- Connector topology, the KWin external-brightness provider, adaptive
  brightness, DDC/CI, live package adoption, and physical-hardware
  qualification remain outside this decision.
- Focused private-bus and fixture-root tests cover validation, the target rule,
  deferred ordered apply with readback, client preflight and uncertainty, and
  the Settings slider. No row touches `/sys`, a host bus, or hardware.

## Revisit when

- The KWin backlight provider (PB-5) supplies connector topology.
- A privilege-separated brightness API replaces direct sysfs writes.
- Hardware qualification shows the kernel type preference selects the wrong
  panel.

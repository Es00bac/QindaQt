# ADR-0015: Qualify function before resource refinement

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** Session, shell, platform, and release workgroups
- **Supersedes:** The undocumented 500 MiB bring-up target
- **Superseded by:** None

## Context

QindaQt already has qualified component-level nested compositor workflows, but
it does not yet have one repeatable whole-desktop workflow that boots the
compositor, shell, services, and applications together, accepts test input, and
produces screenshots. Treating an aggressive 500 MiB aggregate idle PSS target
as a prerequisite would optimize an incomplete process graph and slow the
first usable integration point.

The isolation contract remains non-negotiable: development evidence must use a
private runtime directory, private buses, a nested Wayland seat, and test-only
input. It must never attach to or manipulate the developer's host desktop.

## Decision

QindaQt first qualifies a bootable and interactable virtual desktop beneath an
isolated parent Wayland compositor. The gate includes deterministic teardown,
captured screenshots, 1920x1080, 1920x1200 WUXGA, and 2560x1440 coverage,
representative DPI/theme/profile variants, and proof that synthetic input is
confined to the nested seat.

During bring-up, the compositor, shell, and default resident services have an
initial aggregate idle PSS ceiling of 1,024 MiB and an average idle CPU target
below 1% on the reference machine. The team records component and aggregate
measurements, but it does not defer functional integration for speculative
optimization below that ceiling. Once the complete nested desktop is stable,
measured profiles drive progressively tighter budgets through a later
superseding decision.

## Consequences

- A virtual desktop that cannot boot, render, accept isolated input, capture
  screenshots, or tear down reliably fails before performance tuning matters.
- The test harness must expose one safe whole-session entry point and retain
  resolution, scale, theme, profile, and multi-output evidence.
- Screenshots and input traces come only from the private nested environment;
  host cursor movement or host-session mutation is a containment failure.
- 1,024 MiB is a ceiling, not a consumption goal. Regressions and obvious waste
  are still repaired during bring-up.
- Release qualification still requires physical hardware, packaging,
  migrations, recovery, upgrade, performance, and accessibility evidence.

## Revisit when

Supersede this decision after the integrated desktop passes repeatable nested
qualification and representative measurements identify a lower ceiling that
can be enforced without removing required product behavior.

# ADR-0106: Accept an equivalent Gentoo Power Profiles provider

- Status: Accepted
- Date: 2026-09-07

## Context

The desktop profile adapter consumes the public Power Profiles D-Bus contract,
including both the modern `org.freedesktop.UPower.PowerProfiles` name and the
legacy `net.hadess.PowerProfiles` name. Gentoo's `sys-apps/tuned[ppd]` installs
`tuned-ppd`, which exports that same contract and soft-blocks
`sys-power/power-profiles-daemon`. A hard dependency on the latter would make
Portage replace an already working tuned provider.

## Decision

The `gui-wm/qindaqt-desktop` ebuild declares the providers as one Gentoo
alternative group: `|| ( sys-apps/tuned[ppd] sys-power/power-profiles-daemon )`.
The adapter remains provider-neutral and uses only the documented D-Bus API;
QindaQt does not start, configure, or inspect either daemon's private state.

## Consequences

Hosts with tuned's `ppd` USE flag retain that provider, while hosts without
either provider can select `power-profiles-daemon`. Release checks and package
documentation must preserve the alternative group. Provider compatibility must
be rechecked if either implementation changes its public names, objects, or
profile/hold methods.

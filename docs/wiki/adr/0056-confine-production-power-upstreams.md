# ADR-0056: Confine production power upstreams behind injected adapters

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Power platform service
- **Supersedes:** ADR-0024's requirement that every internal-brightness write use logind
- **Superseded by:** None

## Context

Power1's resident orchestration and public wire contract were implemented
before any production platform source. Keeping deterministic unavailable
collaborators was safe, but it left packaged clients without battery, profile,
lid, inhibitor, or internal-backlight truth. Direct use of well-known D-Bus
names also cannot fence a multi-call refresh against daemon replacement, and
tests must never discover an ambient system bus or host sysfs tree.

Power1 v1 has profile and keyboard-backlight mutations but no display-
brightness method and no session-action fields. The platform slice still needs
a narrow, testable logind action authority for the later PB-3 shell controller
without adding those values to the Power1 snapshot.

## Decision

The resident composition root selects exactly one explicit upstream mode.
`production` constructs UPower, power-profiles-daemon, logind-session, and
sysfs-backlight adapters from an injected bus connection and injected
backlight root. `unavailable` retains the deterministic PB-1 collaborators and
does not open an upstream bus. The installed D-Bus descriptor and systemd user
unit select production; a bare executable remains unavailable by default so an
unconfigured invocation cannot contact platform authorities.

Every D-Bus refresh resolves a unique owner and publishes only an atomic set of
replies from that owner. Owner loss or replacement withdraws the domain,
advances service epoch where an accepted authority existed, and rejects stale
replies. UPower decoding requires exact property types and known ordinals;
`Online` defines line-power truth, `PowerSupply=false` excludes peripheral
battery or UPS devices from the system inventory, and `IsPresent` is
battery-only.
Power Profiles supports the current
`org.freedesktop.UPower.PowerProfiles` root plus the legacy
`net.hadess.PowerProfiles` root; holds use the daemon's unsigned cookie and
never expose it as a public handle. logind drops inhibitor UID/PID fields and
admits an action only after a fresh exact-owner `Can*` query at dispatch returns
`yes`; the earlier published admission set cannot authorize execution, and
`challenge` never opens a polkit UI.

The sysfs adapter touches only its injected root. It reads bounded decimal
`max_brightness`, `brightness`, and `actual_brightness` values and reports a
read-only brightness file as unavailable. Its narrow write primitive writes
only that file when it is writable, never escalates privileges, and re-reads
observed truth afterward. This direct primitive supersedes ADR-0024's blanket
logind-write requirement; it does not create a Power1 v1 display-brightness
method or change KWin's adaptive-brightness authority.

## Consequences

Production truth is useful without adding libupower or a power-profiles client
library, while the core coordinator continues to consume the same collaborator
interfaces. Private-bus and fixture-root tests exercise the identical adapters
without host access. A systemd sandbox or ordinary permissions may make the
brightness file read-only; that is a supported unavailable result, not a
reason to add setuid, polkit, or fallback paths.

The separate logind action authority is an installed C++ boundary but is not
composed into Power1 v1. PB-3 must own shell presentation and operation
admission before exposing those actions to users. Keyboard-backlight discovery
and mutation, idle hints, the KWin external-brightness provider, and a public
display-brightness mutation remain outside this decision.

## Revisit when

Revisit direct backlight mutation only if Power1 gains a reviewed display-
brightness method or a privilege-separated platform API with explicit subject,
lineage, cancellation, and no-prompt behavior. Revisit session-action exposure
only with the PB-3 shell controller contract.

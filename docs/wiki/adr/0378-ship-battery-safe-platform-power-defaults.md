# ADR-0378: Ship battery-safe platform power defaults

- **Status:** Accepted
- **Date:** 2026-10-16
- **Owners:** Power / Packaging
- **Supersedes:** None
- **Superseded by:** None

## Context

Installed QindaQt builds run Power1 with every native policy switched off
(`QINDAQT_NATIVE_POWER_EXCLUSIVE=OFF`), so logind and UPower act on lid, power
key and battery events. The 2026-10-16 stability audit
(`ops/team/specs/2026-10-16-stability/findings.md`) found:

- **A short power-button press powered the laptop off**, because the stock
  default is `HandlePowerKey=poweroff`.
- **Flat batteries.** The laptop's s2idle resumes within seconds of every
  suspend, and logind re-suspends a closed lid about every 31 s (878 cycles in
  one day). Four of six forced power-offs were a drained battery. UPower's
  critical HybridSleep wrote an image, woke spuriously, and discarded it.
- **Suspend hangs.** Since systemd 256, user sessions are frozen before sleep.
  A process blocked in a FUSE request, such as an sshfs network location whose
  network was just taken down, cannot be frozen. Suspend never began and the
  owner had to force a power-off (2026-10-12, 2026-10-14).

## Decision

The overlay package `sys-power/qindaqt-power-defaults` installs vendor
drop-ins. Lid and power-key behaviour follows the owner's decisions of
2026-10-16, which were made for the laptop. The package is installed per
machine, not through the QindaQt profile set, so a workstation keeps the stock
power key until its owner chooses otherwise.

- **logind:**
  - `HandlePowerKey=suspend`
  - `HandlePowerKeyLongPress=poweroff`
  - `HandleLidSwitch=suspend-then-hibernate`
  - `HandleLidSwitchExternalPower=suspend`
  - `HandleLidSwitchDocked=ignore`
- **sleep:** `HibernateDelaySec=90min` and `HibernateOnACPower=no`.
  suspend-then-hibernate also hibernates below 5% battery. It re-checks on
  every cycle logind starts, so spurious wakes can no longer drain the battery.
- **No user-session freeze:** `SYSTEMD_SLEEP_FREEZE_USER_SESSIONS=false` for the
  suspend, hibernate, hybrid-sleep and suspend-then-hibernate services.

These are platform defaults. Once Power1's native policy becomes exclusive, its
Settings1 choices take over through logind inhibitors, and these drop-ins remain
only as the fallback for sessions without QindaQt.

## Consequences

- A closed lid on battery ends in hibernation rather than a dead battery. When
  hibernation is unavailable (for example, not enough free swap), logind falls
  back to an ordinary suspend.
- QindaQt Settings' lid choice is not honoured until native exclusivity ships.
  The owner's stored choice ("none") differs from the behaviour they selected on
  2026-10-16. Settings must be aligned, or the native policy enabled, before the
  Settings page can be trusted.
- The spurious-wake source remains a hardware or firmware defect to diagnose;
  these defaults bound its cost.

## Revisit when

- Power1's native lid and critical policies become exclusive.
- The s2idle wake source is fixed, and plain suspend becomes reliable on battery.
- systemd's user-session freeze handles FUSE waits safely.

# L6 — Packaging and system policy (manager-owned; container-wm data plus the QindaGentoo overlay)

All system-level configuration ships through Portage (owner rule). Per-user files stay user configuration.

## Session bus hardening (container-wm `data/`, installed by the desktop package)
- `/usr/share/dbus-1/session.d/qindaqt-session-limits.conf`: `max_outgoing_bytes` and `max_incoming_bytes` set
  to 67108864 (64 MiB). A client that stops reading (the 10-15 Settings instance) is cut off instead of
  growing dbus-daemon to 4 GB and filling swap.
- `/usr/lib/systemd/user/dbus.service.d/qindaqt-oom.conf`: `OOMPolicy=continue`. An OOM kill of one helper
  inside dbus.service's cgroup must not stop the session bus; that happened on 10-02.
- Settings1 and NightLight get their own systemd units (lane L1), so they leave dbus.service's cgroup.

## Power and sleep defaults (new overlay package `sys-power/qindaqt-power-defaults`)
- `/usr/lib/systemd/system/systemd-{suspend,hibernate,hybrid-sleep,suspend-then-hibernate}.service.d/qindaqt-no-freeze.conf`:
  `Environment=SYSTEMD_SLEEP_FREEZE_USER_SESSIONS=false`. Freezing user sessions together with FUSE/sshfs
  network locations hung suspend on 10-12 and 10-14.
- `/usr/lib/systemd/logind.conf.d/qindaqt-power.conf`:
  - `HandlePowerKey=suspend`; the stock default is poweroff, so a short press shut the laptop down.
  - `HandlePowerKeyLongPress=poweroff`.
  - `HandleLidSwitch=suspend-then-hibernate`.
  - `HandleLidSwitchExternalPower=suspend`.
  - `HandleLidSwitchDocked=ignore`.
- `/usr/lib/systemd/sleep.conf.d/qindaqt-sleep.conf`: `HibernateDelaySec=` sized so a lid-closed laptop
  hibernates before it drains. On this hardware, s2idle wakes within seconds.
- UPower critical action `Hibernate` (pure S4, which cannot spuriously wake), via a `UPower.conf.d` drop-in if
  upower-1.90.9 supports one; otherwise the manager documents why and leaves it.
- An owner-assisted diagnostic script, run once at a convenient time, to identify the s2idle wake source. It
  uses pm_debug_messages, `/sys/power/pm_wakeup_irq` and per-source `wakeup_count`. Any resulting quirk ships
  as a package file (udev rule or boot parameter through qinda-boot), never as a hand edit.

## Gabbee interplay (app-accessibility/gabbee)
- Remove gabbee's `90-gabbee-recovery.conf` drop-ins on QindaQt units. They impose `StartLimitBurst=2`/60 s,
  so a QindaQt service can stay dead until re-login. QindaQt units own their own restart policy.

## Release (once, at the end)
- A new `gui-wm/qindaqt-desktop` revision built entirely from one integrated container-wm commit. This ends
  the R23/R25/R28/R29 binary patchwork.
- A new `gui-wm/qindaqt-kwin` revision from the integrated fork commit.
- Build on qinda, verify, install on the laptop through normal Portage, then the owner re-logs in.

# ADR-0377: Keep the session bus alive through helper OOM kills

- **Status:** Accepted
- **Date:** 2026-10-16
- **Owners:** Session
- **Supersedes:** None
- **Superseded by:** None

## Context

D-Bus-activated helpers that have no `SystemdService=` run inside the user
`dbus.service` cgroup. The stock unit sets `OOMPolicy=stop`, so when the kernel
OOM killer chooses any one of those helpers, systemd stops the whole session bus.
On 2026-10-02 this happened, and the shell crashed three seconds later in its
`QDBusConnection` thread. The stability round's findings are recorded in
`ops/team/specs/2026-10-16-stability/findings.md`.

The same audit found one stalled client, a Settings window that stopped reading
its socket, making `dbus-daemon` buffer about 4 GB and fill swap. A queue limit
looked like the natural cap. We measured it on a private daemon with a
subscriber that never reads and a publisher flooding 64 KiB signals:

- **Stock limits (1 GB):** the daemon's resident memory grew in step with the
  flood (304 MiB after 288 MiB). This is the 4 GB mechanism.
- **64 MiB `max_incoming_bytes`/`max_outgoing_bytes`:** the daemon stopped near
  72 MiB, but it then stopped *reading from the publisher*. The publisher's next
  write blocked, because its messages were held behind the stalled subscriber.
  The same back-pressure happens at the stock 1 GB once enough of one publisher's
  messages are stuck.

## Decision

- QindaQt installs a `dbus.service.d` drop-in that sets `OOMPolicy=continue`.
  The OOM killer then removes only the chosen process, and the bus survives.
- QindaQt does **not** lower dbus-daemon queue limits, because a lower limit
  blocks healthy publishers sooner.
- Stalled-subscriber growth is fixed at the source:
  - clients subscribe to high-rate signals only while a consumer is visible
    (Audio1 Levels);
  - publishers coalesce bursts (window and task-list changes);
  - change signals are emitted only on real change (Sleep1);
  - the Settings stall itself is repaired.
- QindaQt resident helpers get their own systemd user units (Settings1,
  NightLight), so they no longer share `dbus.service`'s cgroup.

## Consequences

- An OOM event can no longer take every desktop connection down at once.
- A client that stops reading still makes the daemon buffer messages for it.
  The amount is bounded by the signal rates the client subscribed to, which this
  round reduces. A future publisher of high-rate data must use a pull model or an
  on-demand subscription, never an always-on broadcast.
- The drop-in applies to every user session bus on the machine, including
  non-QindaQt sessions. That is safe, because it only changes what systemd does
  after an OOM kill.

## Revisit when

- dbus-daemon gains a per-connection limit that disconnects or drops for a slow
  *receiver* without throttling its senders.
- QindaQt moves to a bus broker with different overload semantics.

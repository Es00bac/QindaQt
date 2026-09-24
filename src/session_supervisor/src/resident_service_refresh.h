// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QtDBus/QDBusConnection>

namespace QindaQt::SessionSupervisor {

// This fixed list contains services that cache desktop session state across
// logins, plus the narrow Audio1 package-upgrade exception: a persistent user
// manager can retain its old D-Bus ABI after the installed binary changes.
// AGENT-NOTE: source of truth is each service's owning module.
// The backend restart is requested first; request order does not guarantee
// completion order, since each `RestartUnit` call only enqueues a systemd
// job: `plasma-xdg-desktop-portal-kde` (the KDE portal
// backend; a Qt Wayland client and screencast/remote-desktop consumer, not
// D-Bus-only) and `xdg-desktop-portal` (the portal frontend, which selects
// and caches which backend it routes to, from `XDG_CURRENT_DESKTOP`, once at
// its own startup rather than per call; ADR-0094). The other two hold their
// own direct Wayland connection: `qindaqt-clipboard-host` (ADR-0058,
// wl_display_connect in clipboard_wayland_adapter) and
// `qindaqt-display-service` (ADR-0053, wl_display_connect in
// display_writer's output-management port). Ordinary D-Bus-only services
// remain excluded unless a specific lifecycle defect is demonstrated, as it
// is for Audio1's upgrade-stale process.
[[nodiscard]] QStringList residentServiceRefreshUnits();

// Requests a restart of each named systemd user unit by calling
// `RestartUnit(name, "replace")` on the systemd user manager — through its
// private control socket when it exists, otherwise through the supplied
// session bus when the manager actually owns org.freedesktop.systemd1 there
// — in the order given. `systemdPrivateSocketPath` overrides the socket
// location for hermetic tests. Call after publishActivationEnvironment and before starting desktop
// consumers. AGENT-CONTRACT: a unit already resident from a prior desktop
// keeps its stale Wayland connection or cached routing until the process
// itself restarts; SetEnvironment only changes the environment future
// activations receive. `RestartUnit` enqueues a systemd job and returns a job
// object path; it does not wait for the restart to finish. It both requests a
// restart for an already-active unit and starts one that is not yet running.
// For Audio1, this function waits up to two seconds for its old unique owner
// to disappear. It returns false if that owner cannot be proven retired;
// the helper logs this failure and keeps the desktop available. Other unit
// restart failures remain best-effort, and no unit outside the fixed list is
// touched.
bool refreshResidentServices(const QDBusConnection &bus,
                             const QStringList &unitNames,
                             const QString &systemdPrivateSocketPath = {});

} // namespace QindaQt::SessionSupervisor

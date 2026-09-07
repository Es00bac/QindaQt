// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QtDBus/QDBusConnection>

namespace QindaQt::SessionSupervisor {

// The fixed, reviewed set of systemd user units that can be resident from a
// prior desktop with session-scoped state that `SetEnvironment` alone cannot
// refresh, and so must be explicitly restarted when this session starts, in
// the listed order. AGENT-NOTE: source of truth is each service's own module.
// Listed backend-first so a restarted frontend re-selects against an
// already-refreshed backend: `plasma-xdg-desktop-portal-kde` (the KDE portal
// backend; a Qt Wayland client and screencast/remote-desktop consumer, not
// D-Bus-only) and `xdg-desktop-portal` (the portal frontend, which selects
// and caches which backend it routes to, from `XDG_CURRENT_DESKTOP`, once at
// its own startup rather than per call; ADR-0094). The other two hold their
// own direct Wayland connection: `qindaqt-clipboard-host` (ADR-0058,
// wl_display_connect in clipboard_wayland_adapter) and
// `qindaqt-display-service` (ADR-0053, wl_display_connect in
// display_writer's output-management port). Add a unit here only when its
// module independently opens Wayland, is itself a Wayland client, or
// otherwise caches session-scoped state at startup; ordinary D-Bus-only
// services with no such cache pick up the republished environment on their
// next systemd activation and need no entry.
[[nodiscard]] QStringList residentServiceRefreshUnits();

// Requests a restart of each named systemd user unit by calling
// `RestartUnit(name, "replace")` on the supplied session bus, in the order
// given. Call after publishActivationEnvironment and before starting desktop
// consumers. AGENT-CONTRACT: a unit already resident from a prior desktop
// keeps its stale Wayland connection or cached routing until the process
// itself restarts; SetEnvironment only changes the environment future
// activations receive. `RestartUnit` enqueues a systemd job and returns a job
// object path; it does not wait for the restart to finish. It both requests a
// restart for an already-active unit and starts one that is not yet running,
// so no separate state query is needed. Each D-Bus call itself (not the
// restart job) is bounded by a short timeout, and is best-effort: a missing
// unit or transport failure is logged and does not stop the session, and no
// unit outside the fixed list is touched.
void refreshResidentServices(const QDBusConnection &bus,
                             const QStringList &unitNames);

} // namespace QindaQt::SessionSupervisor

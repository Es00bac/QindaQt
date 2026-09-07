// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QtDBus/QDBusConnection>

namespace QindaQt::SessionSupervisor {

// The fixed, reviewed set of systemd user units that can be resident from a
// prior desktop with session-scoped state that `SetEnvironment` alone cannot
// refresh, and so must be explicitly restarted when this session starts.
// AGENT-NOTE: source of truth is each service's own module. Two hold their
// own direct Wayland connection (not merely a D-Bus client): currently
// `qindaqt-clipboard-host` (ADR-0058, wl_display_connect in
// clipboard_wayland_adapter) and `qindaqt-display-service` (ADR-0053,
// wl_display_connect in display_writer's output-management port). Two more
// are D-Bus-only but cache desktop identity/routing decisions made at their
// own startup rather than per call: `xdg-desktop-portal` (the portal
// frontend, which selects and caches its backend from
// `XDG_CURRENT_DESKTOP`) and `plasma-xdg-desktop-portal-kde` (the KDE portal
// backend it routes to; ADR-0094). Add a unit here only when its module
// independently opens Wayland or otherwise caches session-scoped state at
// startup; ordinary D-Bus-only services with no such cache pick up the
// republished environment on their next systemd activation and need no
// entry.
[[nodiscard]] QStringList residentServiceRefreshUnits();

// Restarts each named systemd user unit through `RestartUnit(name, "replace")`
// on the supplied session bus. Call after publishActivationEnvironment and
// before starting desktop consumers. AGENT-CONTRACT: a unit already resident
// from a prior desktop keeps its stale Wayland connection or cached routing
// until the process itself restarts; SetEnvironment only changes the
// environment future activations receive. RestartUnit both restarts an
// already-active unit and starts one that is not yet running, so no separate
// state query is needed. Each call is bounded and best-effort: a missing unit
// or transport failure is logged and does not stop the session, and no unit
// outside the fixed list is touched.
void refreshResidentServices(const QDBusConnection &bus,
                             const QStringList &unitNames);

} // namespace QindaQt::SessionSupervisor

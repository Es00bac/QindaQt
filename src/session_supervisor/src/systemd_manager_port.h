// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QtDBus/QDBusConnection>

namespace QindaQt::SessionSupervisor {

// How a session-supervisor call reaches the systemd user manager.
//
// AGENT-NOTE: org.freedesktop.systemd1 on the session bus is the manager only
// when the session bus itself is the systemd user bus. QindaQt's session
// bootstraps its own private bus with dbus-run-session when no bus is
// provided; on that bus the name is unowned (and D-Bus activation of it would
// spawn a second, failing user manager — the production "Could not update the
// user service activation environment" failure). The manager's own endpoint
// is always reachable through sd-bus (the same library systemctl uses); its
// private control socket is NOT a message bus and never answers Hello, so a
// QtDBus connection to it deadlocks — the call must go through sd-bus.
struct SystemdManagerRoute {
    enum class Kind {
        // Call the manager directly through sd-bus (private control socket).
        Native,
        // Call org.freedesktop.systemd1 over the supplied session bus; the
        // name is verified registered there first.
        SessionBusName,
        // Neither path exists; report and skip.
        Unavailable,
    };

    Kind kind = Kind::Unavailable;
    QString address;
    // True only for hermetic-test override endpoints, which are ordinary
    // message buses that require the client Hello handshake before any
    // method call; the production private endpoint forbids it.
    bool requiresBusHello = false;
};

// Resolves the manager route. `privateSocketPath` overrides the computed
// endpoint for hermetic tests (an sd-bus connection is made to that explicit
// address, which may name a fake manager bus); an empty value uses
// QStandardPaths::RuntimeLocation/systemd/private. The native path wins
// whenever its socket exists; the session-bus name is used only when it is
// actually registered, so a private-bus session never triggers a spurious
// second-user-manager activation.
[[nodiscard]] SystemdManagerRoute resolveSystemdManagerRoute(
    const QDBusConnection &sessionBus, const QString &privateSocketPath = {});

// Sends org.freedesktop.systemd1.Manager.SetEnvironment(assignments) through
// sd-bus on the given route address (or the manager's default endpoint when
// the address is empty). `requiresBusHello` marks a hermetic-test bus
// endpoint; the manager's private endpoint never takes a Hello. Returns
// false on any connection or call failure.
[[nodiscard]] bool nativeSetManagerEnvironment(const QString &address,
                                               const QStringList &assignments,
                                               bool requiresBusHello = false);

// Sends org.freedesktop.systemd1.Manager.RestartUnit(unitName, "replace")
// through sd-bus on the given route address (or the manager's default
// endpoint when the address is empty). Returns false on any connection or
// call failure.
[[nodiscard]] bool nativeRestartUnit(const QString &address, const QString &unitName,
                                     bool requiresBusHello = false);

} // namespace QindaQt::SessionSupervisor

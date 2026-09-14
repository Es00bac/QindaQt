// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QProcessEnvironment>
#include <QtDBus/QDBusConnection>

namespace QindaQt::SessionSupervisor {
// Called after KWin supplies its socket and before any desktop consumers start.
// The daemon's activation environment is updated over the supplied session
// bus; the systemd user manager's SetEnvironment is routed to the manager's
// private control socket whenever it exists (the session-bus name is used
// only when the manager actually owns it, never activating a second user
// manager on a private-bus session). Failures are reported and do not stop
// the desktop. Only desktop connection variables and explicit Qt appearance
// selections are exported. `systemdPrivateSocketPath` overrides the socket
// location for hermetic tests.
void publishActivationEnvironment(const QDBusConnection &bus,
                                  const QProcessEnvironment &environment,
                                  const QString &systemdPrivateSocketPath = {});
}

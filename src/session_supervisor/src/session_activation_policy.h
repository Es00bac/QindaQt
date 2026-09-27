// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>
#include <QtTypes>

namespace QindaQt::SessionSupervisor {
// Private is the default: a caller must establish a physical compositor
// witness before allowing any contact with the shared user manager.
enum class SessionActivationScope { Private, PhysicalDesktop };

// Pure policy over the witnessed executable and its argv. When a capability-
// bearing KWin hides /proc/exe, exact comm plus argv[0] identity is required.
// This guards accidental session interference, not hostile same-user code. Only KWin's own
// options before --exit-with-session count; payload arguments are untrusted.
[[nodiscard]] SessionActivationScope activationScopeForCompositor(
    const QString &executable, const QStringList &arguments,
    const QString &processName = {});

// Reads the already lifetime-witnessed direct parent's /proc executable and
// command line, falling back to bounded comm evidence if exe is inaccessible.
// Missing/incomplete evidence fails closed to Private.
[[nodiscard]] SessionActivationScope witnessedSessionActivationScope(qint64 compositorPid);
}

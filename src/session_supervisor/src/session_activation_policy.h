// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>
#include <QtTypes>

namespace QindaQt::SessionSupervisor {
// Private is the default: a caller must establish a physical compositor
// witness before allowing any contact with the shared user manager.
enum class SessionActivationScope { Private, PhysicalDesktop };

// Pure policy over the witnessed executable and its argv. Only KWin's own
// options before --exit-with-session count; payload arguments are untrusted.
[[nodiscard]] SessionActivationScope activationScopeForCompositor(
    const QString &executable, const QStringList &arguments);

// Reads the already lifetime-witnessed direct parent's /proc executable and
// command line. Missing/incomplete evidence fails closed to Private.
[[nodiscard]] SessionActivationScope witnessedSessionActivationScope(qint64 compositorPid);
}

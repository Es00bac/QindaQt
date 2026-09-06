// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QProcessEnvironment>
#include <QtDBus/QDBusConnection>

namespace QindaQt::SessionSupervisor {
// Called after KWin supplies its socket and before any desktop consumers start.
// Uses the supplied session bus only; failures are reported and do not stop the
// desktop. No values other than desktop connection variables are exported.
void publishActivationEnvironment(const QDBusConnection &bus,
                                  const QProcessEnvironment &environment);
}

// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::SystemMonitor {

[[nodiscard]] QString applyProcessAction(const QString &procRoot, qint64 pid,
                                         quint64 startTicks,
                                         const QString &action, int value);

} // namespace QindaQt::SystemMonitor

// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

class QProcess;

namespace QindaQt::SessionSupervisor {

class SupervisedProcessLauncher final {
public:
    // Starts a non-secret child without waiting for readiness. The QProcess
    // reports exec failure asynchronously, while the child is still bound to
    // the supervisor through the same race-closed parent-death contract used
    // for essential tokenized children.
    [[nodiscard]] static bool start(QProcess &process, const QString &program,
                                    const QStringList &arguments = {},
                                    QString *error = nullptr);
};

} // namespace QindaQt::SessionSupervisor

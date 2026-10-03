// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QSet>
#include <QStringList>

namespace QindaQt::Controllers {
struct GamePriorityState {
    bool steam = false;
    QSet<QString> busyPaths;
};
// Linux observation only: never grabs a controller or modifies a game. One
// scan collects all same-user consumers. Injected roots make pre-existing
// opens and Steam startup reproducible without touching live processes.
GamePriorityState inspectGamePriority(const QSet<QString> &paths,
                                     const QString &procRoot = "/proc",
                                     qint64 selfPid = -1);
QStringList controllerDevicePaths(quint16 vendor, quint16 product,
                                 const QString &sdlPath,
                                 const QString &sysRoot = "/sys",
                                 const QString &devRoot = "/dev");
} // namespace QindaQt::Controllers

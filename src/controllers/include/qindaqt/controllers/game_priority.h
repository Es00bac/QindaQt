// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

namespace QindaQt::Controllers {
// One observation scan of who is using the watched controller nodes
// (ADR-0347, ADR-0397, ADR-0428).
struct GamePriorityState {
    bool steam = false;
    // Watched nodes held open by any other same-user process: a browser, a
    // remapper or a game. Informational; holding a node does not pause control.
    QSet<QString> busyPaths;
    // The subset of busyPaths held by a process that is not a passive web-engine
    // host and belongs to the application that owns the focused window. Only
    // these pause desktop control (reason "game").
    QSet<QString> gamePaths;
    // True when any holder of a watched node is not a passive web-engine host,
    // focused or not. While false, no focus change can alter gamePaths; the
    // observer uses this to leave held desktop input alone on a focus change.
    bool consumerPresent = false;
    // Short printable process name for each gamePaths entry, for diagnostics
    // only. Never an identifier, path or command; empty when unreadable.
    QHash<QString, QString> gameConsumers;
};
struct GamePriorityQuery {
    QSet<QString> paths;
    // Process that owns the focused window, as the compositor reports it.
    // Non-positive means no focused application, so nothing can be related.
    qint64 focusedPid = 0;
    // Process excluded from the scan; negative means the calling process.
    qint64 selfPid = -1;
    // The compositor's own session id and cgroup. Services started by the
    // session share both, so they must never relate two applications. Unset
    // reads the running process (getsid(0), /proc/self/cgroup); tests inject.
    std::optional<qint64> compositorSession;
    std::optional<QString> compositorCgroup;
};
// Linux observation only: never grabs a controller or modifies a game. One
// scan collects all same-user holders of `query.paths` and classifies each
// against the focused application (ADR-0428). Injected roots make pre-existing
// opens, Steam startup and process relations reproducible without touching
// live processes. Reads FD link text without following targets; reads
// /proc/<pid>/maps only for holders, environ/cgroup/stat for holders and the
// focused process, and stat for their ancestors, all bounded; cooperative
// QThread interruption is checked between entries. Call off the GUI thread for
// real process trees.
GamePriorityState inspectGamePriority(const GamePriorityQuery &query, const QString &procRoot = "/proc");
// Focus-agnostic scan: Steam and busyPaths only. With no focused application
// gamePaths is always empty.
GamePriorityState inspectGamePriority(const QSet<QString> &paths,
                                     const QString &procRoot = "/proc",
                                     qint64 selfPid = -1);
QStringList controllerDevicePaths(quint16 vendor, quint16 product,
                                 const QString &sdlPath,
                                 const QString &sysRoot = "/sys",
                                 const QString &devRoot = "/dev");
} // namespace QindaQt::Controllers

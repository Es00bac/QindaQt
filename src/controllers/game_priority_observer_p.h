// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/controllers/game_priority.h"
#include <QThread>
#include <functional>

namespace QindaQt::Controllers {
// Private observation worker: SDL, input delivery and profiles never leave
// ControllerRuntime's thread. Destruction interrupts and joins the one scan.
//
// Focus (ADR-0428): the runtime names the process that owns the focused window
// and the worker classifies holders against it. Every /proc read stays on the
// worker; setFocusedProcess() only records a pid and schedules a scan.
class GamePriorityObserver final : public QObject {
    Q_OBJECT
public:
    // Runs on the worker thread; must be safe to call concurrently with the
    // GUI thread and must poll QThread::isInterruptionRequested().
    using Inspector = std::function<GamePriorityState(const GamePriorityQuery &, const QString &procRoot)>;
    // An empty Inspector selects inspectGamePriority().
    explicit GamePriorityObserver(QString procRoot, Inspector inspect = {});
    ~GamePriorityObserver() override;
    void refresh(QSet<QString> paths, bool invalidate = false);
    // Process that owns the focused window; non-positive when there is none.
    // Always requests a fresh scan with the new pid, but emits invalidated()
    // only when the last published observation had a holder that is not a
    // passive web-engine host. With none, a focus change cannot alter the
    // verdict and held desktop input must survive it.
    void setFocusedProcess(qint64 pid);
Q_SIGNALS:
    void invalidated();
    void observed(const QindaQt::Controllers::GamePriorityState &state);
private:
    void scan();
    QString m_procRoot;
    Inspector m_inspect;
    QSet<QString> m_paths;
    quint64 m_generation = 0;
    qint64 m_focusedPid = 0;
    // consumerPresent of the last published observation (GUI thread only).
    bool m_consumerPresent = false;
    bool m_scanning = false;
    QThread m_thread;
    QObject *m_worker;
};
} // namespace QindaQt::Controllers

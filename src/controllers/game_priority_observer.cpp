// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_priority_observer_p.h"
#include <QCoreApplication>
#include <pthread.h>
#include <sched.h>

namespace QindaQt::Controllers {
GamePriorityObserver::GamePriorityObserver(QString procRoot, Inspector inspect)
    : m_procRoot(std::move(procRoot)), m_inspect(std::move(inspect)), m_worker(new QObject) {
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.setObjectName("controller-observation");
    m_thread.start(QThread::LowPriority);
}
GamePriorityObserver::~GamePriorityObserver() {
    m_thread.requestInterruption();
    m_thread.quit();
    // AGENT-GUARD: plugin code must remain mapped until the worker has left it.
    // Proc inspection checks interruption between entries and never follows FDs.
    m_thread.wait();
}
void GamePriorityObserver::refresh(QSet<QString> paths, bool invalidate) {
    if (invalidate || paths != m_paths) {
        m_paths = std::move(paths);
        ++m_generation;
        Q_EMIT invalidated();
    }
    // Periodic polls coalesce; a topology/access invalidation fences the old
    // result and requires one scan of the latest paths before input resumes.
    if (!m_scanning) scan();
}
void GamePriorityObserver::scan() {
    m_scanning = true;
    const auto generation = m_generation;
    const auto paths = m_paths;
    const auto root = m_procRoot;
    const auto inspect = m_inspect;
    const auto self = QCoreApplication::applicationPid();
    QMetaObject::invokeMethod(m_worker, [this, generation, paths, root, inspect, self] {
        // The compositor may be realtime. Observation must not inherit its
        // scheduling policy and compete with rendering or input dispatch.
        const sched_param normal{};
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &normal);
        const auto state = inspect(paths, root, self);
        QMetaObject::invokeMethod(this, [this, generation, state] {
            m_scanning = false;
            if (generation != m_generation) {
                scan();
                return;
            }
            Q_EMIT observed(state);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
} // namespace QindaQt::Controllers

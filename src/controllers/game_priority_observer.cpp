// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_priority_observer_p.h"
#include <QCoreApplication>
#include <algorithm>
#include <pthread.h>
#include <sched.h>

namespace QindaQt::Controllers {
namespace {
GamePriorityState inspectProc(const GamePriorityQuery &query, const QString &root) { return inspectGamePriority(query, root); }
}
GamePriorityObserver::GamePriorityObserver(QString procRoot, Inspector inspect)
    : m_procRoot(std::move(procRoot)), m_inspect(inspect ? std::move(inspect) : Inspector(inspectProc)), m_worker(new QObject) {
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
void GamePriorityObserver::setFocusedProcess(qint64 pid) {
    pid = std::max<qint64>(pid, 0);
    if (pid == m_focusedPid) return;
    m_focusedPid = pid;
    // Without a watched node there is no holder to classify, and Steam
    // detection ignores focus; the next refresh() carries the new pid.
    if (m_paths.isEmpty()) return;
    if (m_consumerPresent) {
        // AGENT-GUARD (ADR-0428): a game may now own the focus. Release held
        // desktop input and wait for a scan that knows the new focus before the
        // game can receive leaked desktop input; fence the in-flight scan, which
        // was computed for the previous focus. With no such holder nothing here
        // may release input: a held Meta-drag that activates another window must
        // keep working.
        ++m_generation;
        Q_EMIT invalidated();
    }
    if (!m_scanning) scan();
}
void GamePriorityObserver::scan() {
    m_scanning = true;
    const auto generation = m_generation;
    const auto focused = m_focusedPid;
    GamePriorityQuery query;
    query.paths = m_paths;
    query.focusedPid = focused;
    query.selfPid = QCoreApplication::applicationPid();
    const auto root = m_procRoot;
    const auto inspect = m_inspect;
    QMetaObject::invokeMethod(m_worker, [this, generation, focused, query, root, inspect] {
        // The compositor may be realtime. Observation must not inherit its
        // scheduling policy and compete with rendering or input dispatch.
        const sched_param normal{};
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &normal);
        const auto state = inspect(query, root);
        QMetaObject::invokeMethod(this, [this, generation, focused, state] {
            m_scanning = false;
            // A result is stale when invalidation fenced it, or when focus moved
            // while it was computed and it names a holder whose relation to the
            // focused application therefore may have changed. A result without
            // such a holder is focus-independent and stays valid.
            if (generation != m_generation || (focused != m_focusedPid && state.consumerPresent)) {
                scan();
                return;
            }
            m_consumerPresent = state.consumerPresent;
            Q_EMIT observed(state);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
} // namespace QindaQt::Controllers

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/controllers/game_priority.h"
#include <QThread>
#include <functional>

namespace QindaQt::Controllers {
// Private observation worker: SDL, input delivery and profiles never leave
// ControllerRuntime's thread. Destruction interrupts and joins the one scan.
class GamePriorityObserver final : public QObject {
    Q_OBJECT
public:
    using Inspector = std::function<GamePriorityState(const QSet<QString> &, const QString &, qint64)>;
    explicit GamePriorityObserver(QString procRoot, Inspector inspect = inspectGamePriority);
    ~GamePriorityObserver() override;
    void refresh(QSet<QString> paths, bool invalidate = false);
Q_SIGNALS:
    void invalidated();
    void observed(const QindaQt::Controllers::GamePriorityState &state);
private:
    void scan();
    QString m_procRoot;
    Inspector m_inspect;
    QSet<QString> m_paths;
    quint64 m_generation = 0;
    bool m_scanning = false;
    QThread m_thread;
    QObject *m_worker;
};
} // namespace QindaQt::Controllers

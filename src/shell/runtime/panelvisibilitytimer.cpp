// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitytimer.h"

#include <QHash>
#include <QTimer>

#include <limits>
#include <utility>

namespace QindaQt::Shell {

class QtPanelVisibilityTimer::Private final {
public:
    QHash<quint64, QTimer *> timers;
    quint64 nextToken = 1;
};

QtPanelVisibilityTimer::QtPanelVisibilityTimer(QObject *parent)
    : QObject(parent)
    , m_private(new Private)
{
}

QtPanelVisibilityTimer::~QtPanelVisibilityTimer()
{
    delete m_private;
}

quint64 QtPanelVisibilityTimer::schedule(
    int delayMilliseconds, std::function<void()> callback)
{
    if (delayMilliseconds < 0 || !callback || m_private->nextToken == 0) {
        return 0;
    }
    const quint64 token = m_private->nextToken;
    m_private->nextToken = token == std::numeric_limits<quint64>::max()
        ? 0
        : token + 1;
    auto *const timer = new QTimer(this);
    timer->setSingleShot(true);
    m_private->timers.insert(token, timer);
    connect(timer, &QTimer::timeout, this,
            [this, token, callback = std::move(callback)]() mutable {
                QTimer *const finished = m_private->timers.take(token);
                if (finished == nullptr) {
                    return;
                }
                finished->deleteLater();
                callback();
            });
    timer->start(delayMilliseconds);
    return token;
}

void QtPanelVisibilityTimer::cancel(quint64 token)
{
    if (QTimer *const timer = m_private->timers.take(token)) {
        timer->stop();
        delete timer;
    }
}

} // namespace QindaQt::Shell

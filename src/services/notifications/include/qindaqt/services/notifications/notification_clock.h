// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QtTypes>

namespace QindaQt::Services::Notifications {

class NotificationClock {
public:
    virtual ~NotificationClock() = default;

    // AGENT-CONTRACT: Values are monotonic milliseconds in one clock domain.
    // Persistence must store wall time separately instead of interpreting this
    // process-local value as an epoch timestamp.
    [[nodiscard]] virtual qint64 nowMs() const noexcept = 0;
};

class SteadyNotificationClock final : public NotificationClock {
public:
    SteadyNotificationClock();

    [[nodiscard]] qint64 nowMs() const noexcept override;

private:
    QElapsedTimer m_timer;
};

} // namespace QindaQt::Services::Notifications

// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/notifications/notification_clock.h"

namespace QindaQt::Services::Notifications {

SteadyNotificationClock::SteadyNotificationClock()
{
    m_timer.start();
}

qint64 SteadyNotificationClock::nowMs() const noexcept
{
    return m_timer.elapsed();
}

} // namespace QindaQt::Services::Notifications

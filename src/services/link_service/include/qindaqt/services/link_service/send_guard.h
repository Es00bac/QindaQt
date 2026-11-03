// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_messages/send_gate.h>
#include <qindaqt/services/link_notifications/notification_presenter.h>

#include <QtCore/QHash>

#include <deque>
#include <functional>

namespace QindaQt::Link {

// Rate limit (5 per minute, 30 per hour, per device) and a notification for
// every accepted Messages.Send. The presenter is not owned and may be null
// (tests / notifications disabled); the rate limit applies regardless.
class SendGuard final : public SendGate {
public:
    using Clock = std::function<qint64()>;  // milliseconds, monotonic
    explicit SendGuard(NotificationPresenter *presenter, Clock clock = {});
    [[nodiscard]] QString admit(const QString &deviceId, int recipients, const QString &requestedBy) override;

    static constexpr int kPerMinute = 5;
    static constexpr int kPerHour = 30;

private:
    NotificationPresenter *m_presenter;
    Clock m_clock;
    QHash<QString, std::deque<qint64>> m_sent;
    quint32 m_lastHandle = 0;
};

} // namespace QindaQt::Link

// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/send_guard.h>
#include <qindaqt/services/link_service/pairing_presenter.h>

#include <QtCore/QElapsedTimer>

namespace QindaQt::Link {
namespace {
constexpr qint64 kMinuteMs = 60'000;
constexpr qint64 kHourMs = 3'600'000;
}

SendGuard::SendGuard(NotificationPresenter *presenter, Clock clock) : m_presenter(presenter), m_clock(std::move(clock))
{
    if (!m_clock) {
        auto timer = std::make_shared<QElapsedTimer>();
        timer->start();
        m_clock = [timer] { return timer->elapsed(); };
    }
}

QString SendGuard::admit(const QString &deviceId, int recipients, const QString &requestedBy)
{
    const qint64 now = m_clock();
    std::deque<qint64> &times = m_sent[deviceId];
    while (!times.empty() && now - times.front() >= kHourMs) {
        times.pop_front();
    }
    int lastMinute = 0;
    for (const qint64 t : times) {
        lastMinute += now - t < kMinuteMs ? 1 : 0;
    }
    if (lastMinute >= kPerMinute || int(times.size()) >= kPerHour) {
        return QStringLiteral("rate-limited");
    }
    times.push_back(now);
    if (m_presenter != nullptr) {
        PresentedNotification n;
        n.appName = QStringLiteral("QindaLink");
        // requestedBy is an executable file name taken from /proc, not phone content, but it is still escaped.
        n.summary = QStringLiteral("QindaLink sent a text to %1 recipient(s)").arg(recipients);
        n.body = QStringLiteral("Requested by %1").arg(sanitizeForNotification(requestedBy));
        m_lastHandle = m_presenter->present(n, 0);
    }
    return {};
}

} // namespace QindaQt::Link

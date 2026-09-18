// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"

#include "qindaqt/services/notification_presentation/presentation_snapshot.h"

#include <QTime>

namespace QindaQt::Services::NotificationPresentationPolicy {
namespace {

constexpr quint32 CriticalUrgency = 2;
constexpr int MinutesPerDay = 24 * 60;

[[nodiscard]] bool isMinuteOfDay(int minutes) noexcept
{
    return minutes >= 0 && minutes < MinutesPerDay;
}

[[nodiscard]] int localMinuteOfDay()
{
    const QTime now = QTime::currentTime();
    return now.isValid() ? now.hour() * 60 + now.minute() : 0;
}

} // namespace

NotificationInterruptionPolicy::NotificationInterruptionPolicy(QObject *parent)
    : QObject(parent)
    , m_clock(&localMinuteOfDay)
{
}

NotificationInterruptionPolicy::QuietHours
NotificationInterruptionPolicy::quietHours() const noexcept
{
    return m_quietHours;
}

void NotificationInterruptionPolicy::setQuietHours(const QuietHours &quietHours)
{
    // AGENT-GUARD: refuse the whole window rather than clamping one end. A
    // clamped end is a time the user never chose, and this decides when the
    // machine goes quiet.
    if (!isMinuteOfDay(quietHours.startMinutes) || !isMinuteOfDay(quietHours.endMinutes)) {
        return;
    }
    if (m_quietHours == quietHours) {
        return;
    }
    m_quietHours = quietHours;
    Q_EMIT quietHoursChanged();
}

void NotificationInterruptionPolicy::setClock(MinuteOfDayClock clock)
{
    m_clock = clock ? std::move(clock) : MinuteOfDayClock(&localMinuteOfDay);
    Q_EMIT quietHoursChanged();
}

bool NotificationInterruptionPolicy::windowContains(const QuietHours &quietHours,
                                                    int minuteOfDay) noexcept
{
    if (!quietHours.enabled || !isMinuteOfDay(minuteOfDay)) {
        return false;
    }
    if (!isMinuteOfDay(quietHours.startMinutes) || !isMinuteOfDay(quietHours.endMinutes)) {
        return false;
    }
    if (quietHours.startMinutes == quietHours.endMinutes) {
        // An empty window, not an all-day one. See the header.
        return false;
    }
    if (quietHours.startMinutes < quietHours.endMinutes) {
        return minuteOfDay >= quietHours.startMinutes && minuteOfDay < quietHours.endMinutes;
    }
    // The window crosses midnight: 22:00 to 07:00 is late evening or early
    // morning, never the afternoon between them.
    return minuteOfDay >= quietHours.startMinutes || minuteOfDay < quietHours.endMinutes;
}

bool NotificationInterruptionPolicy::quietHoursActive() const
{
    return m_clock && windowContains(m_quietHours, m_clock());
}

bool NotificationInterruptionPolicy::doNotDisturbEnabled() const noexcept
{
    return m_doNotDisturbEnabled;
}

void NotificationInterruptionPolicy::setDoNotDisturbEnabled(bool enabled)
{
    if (m_doNotDisturbEnabled == enabled) {
        return;
    }

    m_doNotDisturbEnabled = enabled;
    Q_EMIT doNotDisturbEnabledChanged(enabled);
}

bool NotificationInterruptionPolicy::allowsPopup(
    const NotificationPresentation::PresentationNotification &notification) const
    noexcept
{
    // Quiet hours are the same quieting as the switch, on a schedule: a
    // scheduled quiet period must never be able to hide an emergency, so
    // critical urgency is admitted by both exactly alike.
    if (!m_doNotDisturbEnabled && !quietHoursActive()) {
        return true;
    }

    // AGENT-GUARD: PresentationNotification is publicly constructible even
    // though the wire decoder bounds urgency to 0..2. Fail closed for unknown
    // values while quieting is active so malformed in-process data cannot
    // interrupt.
    return notification.urgency == CriticalUrgency;
}

} // namespace QindaQt::Services::NotificationPresentationPolicy

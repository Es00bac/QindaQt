// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"

#include "qindaqt/services/notification_presentation/presentation_snapshot.h"

namespace QindaQt::Services::NotificationPresentationPolicy {
namespace {

constexpr quint32 CriticalUrgency = 2;

} // namespace

NotificationInterruptionPolicy::NotificationInterruptionPolicy(QObject *parent)
    : QObject(parent)
{
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
    if (!m_doNotDisturbEnabled) {
        return true;
    }

    // AGENT-GUARD: PresentationNotification is publicly constructible even
    // though the wire decoder bounds urgency to 0..2. Fail closed for unknown
    // values while DND is active so malformed in-process data cannot interrupt.
    return notification.urgency == CriticalUrgency;
}

} // namespace QindaQt::Services::NotificationPresentationPolicy

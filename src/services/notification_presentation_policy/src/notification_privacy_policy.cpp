// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"

namespace QindaQt::Services::NotificationPresentationPolicy {

NotificationPrivacyPolicy::NotificationPrivacyPolicy(QObject *parent)
    : QObject(parent)
{
}

bool NotificationPrivacyPolicy::privatePresentationAllowed() const noexcept
{
    return m_privatePresentationAllowed;
}

void NotificationPrivacyPolicy::setPrivatePresentationAllowed(bool allowed)
{
    if (m_privatePresentationAllowed == allowed) {
        return;
    }

    m_privatePresentationAllowed = allowed;
    Q_EMIT privatePresentationAllowedChanged(allowed);
}

} // namespace QindaQt::Services::NotificationPresentationPolicy

// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"
#include <utility>

namespace QindaQt::Services::NotificationPresentationPolicy {

NotificationPrivacyPolicy::NotificationPrivacyPolicy(QObject *parent)
    : NotificationPrivacyPolicy(Admission{}, parent)
{
}

NotificationPrivacyPolicy::NotificationPrivacyPolicy(Admission admission, QObject *parent)
    : QObject(parent), m_admission(std::move(admission))
{
}

bool NotificationPrivacyPolicy::privatePresentationAllowed() const noexcept
{
    if (!m_privatePresentationAllowed) return false;
    try {
        return !m_admission || m_admission();
    } catch (...) {
        return false;
    }
}

void NotificationPrivacyPolicy::setPrivatePresentationAllowed(bool allowed)
{
    if (m_privatePresentationAllowed == allowed) {
        return;
    }

    m_privatePresentationAllowed = allowed;
    Q_EMIT privatePresentationAllowedChanged(privatePresentationAllowed());
}

} // namespace QindaQt::Services::NotificationPresentationPolicy

// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationquietingsettingsbridge.h"

#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"

namespace QindaQt::Shell {

NotificationQuietingSettingsBridge::NotificationQuietingSettingsBridge(
    Services::SettingsClient::SettingsClient &client,
    Services::NotificationPresentationPolicy::NotificationInterruptionPolicy &policy)
    : m_policy(policy), m_controller(client)
{
    // AGENT-GUARD: fail quiet before Settings1 establishes its first exact-
    // owner baseline. A persisted true value must never be briefly violated.
    m_policy.setDoNotDisturbEnabled(true);
    QObject::connect(&m_controller,
                     &Services::SettingsClient::DoNotDisturbController::confirmedValue,
                     &m_controller, [this](bool enabled) {
        // Transport loss emits no confirmed value, so the last accepted policy
        // is retained until a replacement owner publishes a full snapshot.
        m_policy.setDoNotDisturbEnabled(enabled);
    });
}

} // namespace QindaQt::Shell

// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationapplicationsettingsbridge.h"

#include "qindaqt/services/notification_presentation_policy/notification_application_policy.h"
#include "qindaqt/services/settings_client/settings_client.h"

namespace QindaQt::Shell {

NotificationApplicationSettingsBridge::NotificationApplicationSettingsBridge(
    Services::SettingsClient::SettingsClient &client,
    Services::NotificationPresentationPolicy::NotificationApplicationPolicy &policy)
    : QObject(nullptr), m_client(client), m_policy(policy)
{
    QObject::connect(&m_client,
                     &Services::SettingsClient::SettingsClient::snapshotChanged,
                     this,
                     [this] { applySnapshot(); });
    applySnapshot();
}

void NotificationApplicationSettingsBridge::applySnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot.has_value() ||
        m_client.state() != Services::SettingsClient::ClientState::Ready ||
        snapshot->owner != m_client.currentOwner()) {
        // Owner loss or an in-flight replacement does not erase the last
        // confirmed mute rules. A new exact-owner baseline replaces them.
        return;
    }
    // The same strict codec is used by Settings and the presenter. Invalid
    // stored data never partially clears or widens the last confirmed rules.
    if (!m_policy.setSettingsValue(snapshot->values.value(QLatin1String(
            Services::NotificationPresentationPolicy::
                NotificationPoliciesSettingsKey)))) {
        // Invalid snapshots retain the previous confirmed policy in the
        // shared codec; never turn a partial or malformed object into authority.
        return;
    }
}

} // namespace QindaQt::Shell

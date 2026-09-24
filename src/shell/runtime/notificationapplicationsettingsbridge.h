// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Services::NotificationPresentationPolicy {
class NotificationApplicationPolicy;
}

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Shell {

// Projects only the exact confirmed Settings1 value into the separate
// per-application presenter policy. The client and policy are borrowed and
// must remain on this object's construction thread for this bridge's lifetime.
class NotificationApplicationSettingsBridge final : public QObject {
public:
    NotificationApplicationSettingsBridge(
        Services::SettingsClient::SettingsClient &client,
        Services::NotificationPresentationPolicy::NotificationApplicationPolicy &policy);

private:
    void applySnapshot();

    Services::SettingsClient::SettingsClient &m_client;
    Services::NotificationPresentationPolicy::NotificationApplicationPolicy &m_policy;
};

} // namespace QindaQt::Shell

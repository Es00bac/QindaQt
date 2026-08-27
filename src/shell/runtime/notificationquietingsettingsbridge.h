// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_client/do_not_disturb_controller.h"

namespace QindaQt::Services::NotificationPresentationPolicy {
class NotificationInterruptionPolicy;
}

namespace QindaQt::Shell {

class NotificationQuietingSettingsBridge final {
public:
    NotificationQuietingSettingsBridge(
        Services::SettingsClient::SettingsClient &client,
        Services::NotificationPresentationPolicy::NotificationInterruptionPolicy &policy);

    [[nodiscard]] Services::SettingsClient::DoNotDisturbController &controller() noexcept
    { return m_controller; }

private:
    Services::NotificationPresentationPolicy::NotificationInterruptionPolicy &m_policy;
    Services::SettingsClient::DoNotDisturbController m_controller;
};

} // namespace QindaQt::Shell

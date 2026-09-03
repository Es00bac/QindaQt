// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/settings_client/settings_client.h>

#include <optional>

namespace QindaQt::Services::Clipboard {

// Clipboard history is an explicit-consent feature. A resolved schema or
// profile default is configuration truth, but it is not user authorization.
[[nodiscard]] bool hasExplicitHistoryConsent(
    const std::optional<SettingsClient::SettingsSnapshot> &snapshot) noexcept;

} // namespace QindaQt::Services::Clipboard

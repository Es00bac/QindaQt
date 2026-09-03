// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboard_history_consent.h"

#include <QtCore/QMetaType>

namespace QindaQt::Services::Clipboard {
namespace {

constexpr auto kHistoryKey = "services.clipboardHistory";
constexpr auto kUserOverridesLayer = "user-overrides";

} // namespace

bool hasExplicitHistoryConsent(
    const std::optional<SettingsClient::SettingsSnapshot> &snapshot) noexcept
{
    if (!snapshot.has_value()) {
        return false;
    }
    const QVariant value = snapshot->values.value(QLatin1String(kHistoryKey));
    const QVariant source = snapshot->sourceLayers.value(QLatin1String(kHistoryKey));
    // AGENT-GUARD: A true SystemDefaults/ProfileDefaults value is not consent.
    // Settings1 and Clipboard1 share this exact source-layer trust boundary.
    return value.metaType().id() == QMetaType::Bool && value.toBool()
        && source.metaType().id() == QMetaType::QString
        && source.toString() == QLatin1String(kUserOverridesLayer);
}

} // namespace QindaQt::Services::Clipboard

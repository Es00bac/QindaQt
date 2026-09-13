// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_window_preview.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/decoration_painter/decoration_painter.h"
#include "qindaqt/services/settings_client/settings_client.h"

namespace QindaQt::Apps::SettingsCustomize {

using AppAppearance::ApplicationAppearanceController;
using Services::SettingsClient::ClientState;
using Services::SettingsClient::SettingsClient;

CustomizeWindowPreview::CustomizeWindowPreview(
    ApplicationAppearanceController &appearance,
    SettingsClient &preferencesClient, QObject *parent)
    : QObject(parent)
    , m_appearance(appearance)
    , m_preferencesClient(preferencesClient)
{
    connect(&m_appearance, &ApplicationAppearanceController::appearanceChanged,
            this, [this] { recompute(); });
    connect(&m_preferencesClient, &SettingsClient::snapshotChanged, this,
            [this] { recompute(); });
    connect(&m_preferencesClient, &SettingsClient::stateChanged, this,
            [this] { recompute(); });
    recompute();
}

void CustomizeWindowPreview::recompute()
{
    const auto &snapshot = m_preferencesClient.snapshot();
    const Decoration::ChromePreferences preferences =
        (m_preferencesClient.state() == ClientState::Ready && snapshot.has_value())
            ? Decoration::ChromePreferences::fromSettingsValues(snapshot->values)
            : Decoration::ChromePreferences{};
    const QVariantMap next =
        Decoration::resolveWindowChrome(m_appearance.theme(), preferences)
            .toVariantMap();
    if (next == m_chrome) {
        return;
    }
    m_chrome = next;
    Q_EMIT changed();
}

} // namespace QindaQt::Apps::SettingsCustomize

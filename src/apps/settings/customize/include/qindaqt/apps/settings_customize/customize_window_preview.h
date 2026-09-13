// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantMap>

namespace QindaQt::AppAppearance {
class ApplicationAppearanceController;
}
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Apps::SettingsCustomize {

// Read-only projection of the live window-decoration truth onto the
// Customize canvas's contained-window preview. Reuses
// AppAppearance::ApplicationAppearanceController for the confirmed theme
// (the same resolution the compositor's own appearance publisher uses) and
// QindaQt::Decoration::resolveWindowChrome for the identical chrome
// flattening every live decoration paints from, so this preview can never
// drift from what a real contained window renders. Owns no Settings1 write
// authority and never mutates the live compositor.
class CustomizeWindowPreview final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap chrome READ chrome NOTIFY changed)

public:
    // AGENT-CONTRACT: appearance and preferencesClient must outlive this
    // GUI-thread object. appearance already resolves the confirmed
    // `appearance.theme` value independent of this preview (ADR-0074
    // precedent); preferencesClient must be scoped to exactly
    // Decoration::ChromePreferences::settingsKeys(). Before that client
    // confirms a snapshot, chrome reflects the theme's own defaults --
    // never an invented placeholder -- matching appearance's own
    // construction-time fallback.
    CustomizeWindowPreview(
        AppAppearance::ApplicationAppearanceController &appearance,
        Services::SettingsClient::SettingsClient &preferencesClient,
        QObject *parent = nullptr);

    [[nodiscard]] QVariantMap chrome() const { return m_chrome; }

Q_SIGNALS:
    void changed();

private:
    void recompute();

    AppAppearance::ApplicationAppearanceController &m_appearance;
    Services::SettingsClient::SettingsClient &m_preferencesClient;
    QVariantMap m_chrome;
};

} // namespace QindaQt::Apps::SettingsCustomize

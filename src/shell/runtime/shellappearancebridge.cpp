// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellappearancebridge.h"

#include "../common/shelltokenpublisher.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QDebug>

namespace QindaQt::Shell {

ShellAppearanceBridge::ShellAppearanceBridge(
    Services::SettingsClient::SettingsClient &settings,
    Themes::ThemeCatalog &themes, ShellTokenPublisher &tokens,
    bool themeLockedByCli, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_themes(themes)
    , m_tokens(tokens)
    , m_themeLockedByCli(themeLockedByCli)
{
    connect(&m_settings, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &ShellAppearanceBridge::applySnapshot);
}

void ShellAppearanceBridge::applySnapshot()
{
    const auto &snapshot = m_settings.snapshot();
    if (!snapshot.has_value()) {
        return;
    }
    QString decodeError;
    const auto values =
        ShellPreferenceValues::fromVariantMap(snapshot->values, &decodeError);
    if (!values.has_value()) {
        // Keep the last confirmed safe state: a partial or mistyped snapshot
        // must not strip accessibility preferences from a running shell.
        qWarning().noquote()
            << "QindaQt shell ignored an invalid preference snapshot:"
            << decodeError;
        return;
    }
    m_lastConfirmed = *values;
    m_tokens.setFontFamilyOverride(values->fontFamily);
    m_tokens.setAccessibilityInputs(values->accessibility);

    if (!m_themeLockedByCli) {
        const QString currentId = m_themes.current()
                                      .value(QStringLiteral("id"))
                                      .toString();
        if (values->themeId != currentId && !m_themes.selectById(values->themeId)) {
            // AGENT-GUARD: A saved theme that is not installed must fail closed
            // to the running theme, never to a missing-token surface set.
            qWarning().noquote()
                << "QindaQt shell kept theme" << currentId
                << "; preferred theme is not installed:" << values->themeId;
        }
    }
    emit confirmedPreferencesChanged();
}

} // namespace QindaQt::Shell

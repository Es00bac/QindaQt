// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellappearancebridge.h"
#include "qindaqt/app_appearance/appearance_resolver.h"

#include "../common/shelltokenpublisher.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QDebug>
#include <QGuiApplication>
#include <QStyleHints>

namespace QindaQt::Shell {

ShellAppearanceBridge::ShellAppearanceBridge(
    Services::SettingsClient::SettingsClient &settings,
    Themes::ThemeCatalog &themes, ShellTokenPublisher &tokens,
    bool themeLockedByCli, QObject *parent)
    : QObject(parent), m_settings(settings), m_themes(themes), m_tokens(tokens),
      m_themeLockedByCli(themeLockedByCli) {
  connect(&m_settings,
          &Services::SettingsClient::SettingsClient::snapshotChanged, this,
          &ShellAppearanceBridge::applySnapshot);
}

void ShellAppearanceBridge::applySnapshot() {
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
    // The first confirmed snapshot only latches the layout baseline (the
    // startup read already resolved it); later changes are adoption events.
    if (!m_layoutProfileLatched) {
        m_layoutProfileLatched = true;
        m_lastLayoutProfileId = values->layoutProfileId;
    } else if (values->layoutProfileId != m_lastLayoutProfileId) {
        m_lastLayoutProfileId = values->layoutProfileId;
        emit layoutProfilePreferenceChanged(values->layoutProfileId);
    }
    m_lastConfirmed = *values;
    m_tokens.setFontFamilyOverride(values->fontFamily);
    m_tokens.setAccessibilityInputs(values->accessibility);

    if (!m_themeLockedByCli) {
    const QString currentId =
        m_themes.current().value(QStringLiteral("id")).toString();
    const auto scheme =
        AppAppearance::colorSchemeFromToken(values->colorScheme);
    const auto resolved =
        scheme ? AppAppearance::resolveAppearanceTheme(
                     m_themes.themes(),
                     {.themeId = values->themeId, .colorScheme = *scheme},
                     QGuiApplication::styleHints()->colorScheme())
               : std::nullopt;
    const QString resolvedId = resolved ? resolved->id : currentId;
    if (resolvedId != currentId && !m_themes.selectById(resolvedId)) {
            // AGENT-GUARD: A saved theme that is not installed must fail closed
            // to the running theme, never to a missing-token surface set.
      qWarning().noquote() << "QindaQt shell kept theme" << currentId
                           << "; appearance preference could not resolve:"
                           << values->themeId;
        }
    }
    emit confirmedPreferencesChanged();
}

} // namespace QindaQt::Shell

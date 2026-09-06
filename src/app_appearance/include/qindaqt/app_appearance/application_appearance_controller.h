// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/app_appearance/appearance_resolver.h>
#include <qindaqt/themes/theme_spec.h>

#include <QObject>
#include <QStringList>

namespace QindaQt::DesignTokens {
class TokenFacade;
}
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::AppAppearance {

// Projects the confirmed Settings1 appearance.theme value into one validated
// theme. The SettingsClient is borrowed and must outlive this GUI-thread
// controller. Owner loss retains the last validated theme; malformed or
// missing theme identifiers never replace it.
class ApplicationAppearanceController final : public QObject {
  Q_OBJECT
public:
  ApplicationAppearanceController(
      Services::SettingsClient::SettingsClient &settings,
      QStringList themeDirectories, QString fallbackThemeId,
      QString explicitThemeOverride = {}, QObject *parent = nullptr);

  [[nodiscard]] const Themes::ThemeSpec &theme() const noexcept {
    return m_theme;
  }
  [[nodiscard]] const QString &themeId() const noexcept { return m_theme.id; }
  [[nodiscard]] bool hasExplicitOverride() const noexcept {
    return !m_explicitThemeOverride.isEmpty();
  }
  [[nodiscard]] QString lastError() const { return m_lastError; }

  // Publishes the current validated theme to a facade owned by the caller's
  // QML engine. Re-call from appearanceChanged for live QML updates.
  [[nodiscard]] bool publishTokens(DesignTokens::TokenFacade &facade,
                                   QString *error = nullptr) const;

Q_SIGNALS:
  void appearanceChanged();
  void errorChanged();

private:
  void applySnapshot();
  [[nodiscard]] bool selectTheme(const QString &themeId, QString *error);

  Services::SettingsClient::SettingsClient &m_settings;
  QStringList m_themeDirectories;
  QVector<Themes::ThemeSpec> m_installedThemes;
  QString m_explicitThemeOverride;
  Themes::ThemeSpec m_theme;
  QString m_lastError;
};

[[nodiscard]] QStringList
standardThemeDirectories(const QString &explicitDirectory = {});

} // namespace QindaQt::AppAppearance

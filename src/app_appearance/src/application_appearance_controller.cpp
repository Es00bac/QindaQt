// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_appearance/application_appearance_controller.h>

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/themes/theme_loader.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyleHints>

namespace QindaQt::AppAppearance {
namespace {
constexpr auto ThemeKey = "appearance.theme";
constexpr auto SchemeKey = "appearance.colorScheme";

Themes::LoadResult loadTheme(const QString &id,
                             const QStringList &directories) {
  static const QRegularExpression safeId(
      QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
  if (!safeId.match(id).hasMatch()) {
    return {.ok = false,
            .theme = {},
            .error = QStringLiteral("Invalid theme identifier")};
  }
  for (const QString &directory : directories) {
    const QString path = QDir(directory).filePath(id + QStringLiteral(".json"));
    if (QFileInfo::exists(path)) {
      return Themes::ThemeLoader::fromFile(path);
    }
  }
  return {.ok = false,
          .theme = {},
          .error = QStringLiteral("Theme '%1' was not found").arg(id)};
}
} // namespace

QStringList standardThemeDirectories(const QString &explicitDirectory) {
  QStringList result;
  if (!explicitDirectory.isEmpty()) {
    result.append(QFileInfo(explicitDirectory).absoluteFilePath());
  }
  result.append(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                          QStringLiteral("qindaqt/themes"),
                                          QStandardPaths::LocateDirectory));
  result.append(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(QStringLiteral("../share/qindaqt/themes")));
  result.removeDuplicates();
  return result;
}

ApplicationAppearanceController::ApplicationAppearanceController(
    Services::SettingsClient::SettingsClient &settings,
    QStringList themeDirectories, QString fallbackThemeId,
    QString explicitThemeOverride, QObject *parent)
    : QObject(parent), m_settings(settings),
      m_themeDirectories(std::move(themeDirectories)),
      m_explicitThemeOverride(std::move(explicitThemeOverride)) {
  for (const QString &directory : std::as_const(m_themeDirectories)) {
    const QFileInfoList entries = QDir(directory).entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
      const auto loaded =
          Themes::ThemeLoader::fromFile(entry.absoluteFilePath());
      if (!loaded.ok)
        continue;
      bool duplicate = false;
      for (const auto &known : std::as_const(m_installedThemes)) {
        if (known.id == loaded.theme.id) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate)
        m_installedThemes.append(loaded.theme);
    }
  }
  QString error;
  const QString initial = m_explicitThemeOverride.isEmpty()
                              ? std::move(fallbackThemeId)
                              : m_explicitThemeOverride;
  if (!selectTheme(initial, &error)) {
    m_lastError = error;
  }
  connect(&m_settings,
          &Services::SettingsClient::SettingsClient::snapshotChanged, this,
          &ApplicationAppearanceController::applySnapshot);
  if (QGuiApplication::styleHints()) {
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this] { applySnapshot(); });
  }
  applySnapshot();
}

bool ApplicationAppearanceController::selectTheme(const QString &themeId,
                                                  QString *error) {
  const auto loaded = loadTheme(themeId, m_themeDirectories);
  if (!loaded.ok) {
    if (error)
      *error = loaded.error;
    return false;
  }
  if (loaded.theme.id == m_theme.id)
    return true;
  m_theme = loaded.theme;
  emit appearanceChanged();
  return true;
}

void ApplicationAppearanceController::applySnapshot() {
  if (!m_explicitThemeOverride.isEmpty() || !m_settings.snapshot())
    return;
  const QString requested = m_settings.snapshot()
                                ->values.value(QString::fromLatin1(ThemeKey))
                                .toString();
  const auto scheme =
      colorSchemeFromToken(m_settings.snapshot()
                               ->values.value(QString::fromLatin1(SchemeKey))
                               .toString());
  if (requested.isEmpty() || !scheme)
    return;
  const Qt::ColorScheme platformScheme =
      QGuiApplication::styleHints()
          ? QGuiApplication::styleHints()->colorScheme()
          : Qt::ColorScheme::Light;
  const auto resolved = resolveAppearanceTheme(
      m_installedThemes, {.themeId = requested, .colorScheme = *scheme},
      platformScheme);
  if (!resolved)
    return;
  QString error;
  if (selectTheme(resolved->id, &error)) {
    if (!m_lastError.isEmpty()) {
      m_lastError.clear();
      emit errorChanged();
    }
    return;
  }
  if (error != m_lastError) {
    m_lastError = error;
    emit errorChanged();
  }
}

bool ApplicationAppearanceController::publishTokens(
    DesignTokens::TokenFacade &facade, QString *error) const {
  if (m_theme.id.isEmpty()) {
    if (error)
      *error = QStringLiteral("No validated application theme");
    return false;
  }
  return facade.publish(m_theme, {}, error);
}

} // namespace QindaQt::AppAppearance

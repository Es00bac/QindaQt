// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_appearance/application_appearance_controller.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/themes/theme_loader.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyleHints>
#include <cmath>

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
  if (QGuiApplication::styleHints())
    m_platformScheme = QGuiApplication::styleHints()->colorScheme();
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
            this, [this](Qt::ColorScheme scheme) {
              m_platformScheme = scheme;
              applySnapshot();
            });
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
  if (!m_fontFamily.isEmpty()) m_theme.fontFamily = m_fontFamily;
  if (!m_monoFontFamily.isEmpty()) m_theme.monoFontFamily = m_monoFontFamily;
  emit appearanceChanged();
  return true;
}

void ApplicationAppearanceController::applySnapshot() {
  if (!m_settings.snapshot()) return;
  const auto &values = m_settings.snapshot()->values;
  auto inputs = m_accessibility;
  const auto number = [&values](const char *key, double &target) {
    const auto value = values.value(QLatin1String(key));
    if (value.typeId() != QMetaType::Double && value.typeId() != QMetaType::Int
        && value.typeId() != QMetaType::LongLong) return;
    const double n = value.toDouble();
    if (std::isfinite(n)) target = n;
  };
  const auto flag = [&values](const char *key, bool &target) {
    const auto value = values.value(QLatin1String(key));
    if (value.typeId() == QMetaType::Bool) target = value.toBool();
  };
  number("fonts.pointSize", inputs.basePointSize);
  number("accessibility.textScale", inputs.textScale);
  flag("accessibility.reducedMotion", inputs.reducedMotion);
  flag("accessibility.reducedTransparency", inputs.reducedTransparency);
  flag("accessibility.highContrast", inputs.highContrast);
  inputs = inputs.normalized();
  bool changed = inputs != m_accessibility;
  m_accessibility = inputs;
  const auto family = [&values, &changed](const char *key, QString &stored, QString &themeValue) {
    const auto value = values.value(QLatin1String(key));
    if (value.typeId() != QMetaType::QString) return;
    const QString name = value.toString().trimmed();
    if (name.isEmpty() || name.size() > 256 || name == stored) return;
    stored = name;
    themeValue = name;
    changed = true;
  };
  family("fonts.family", m_fontFamily, m_theme.fontFamily);
  family("fonts.monospaceFamily", m_monoFontFamily, m_theme.monoFontFamily);
  // A theme override locks palette choice, not the user's readability needs.
  if (changed) emit appearanceChanged();
  if (!m_explicitThemeOverride.isEmpty()) return;
  const QString requested = m_settings.snapshot()
                                ->values.value(QString::fromLatin1(ThemeKey))
                                .toString();
  const auto scheme =
      colorSchemeFromToken(m_settings.snapshot()
                               ->values.value(QString::fromLatin1(SchemeKey))
                               .toString());
  if (requested.isEmpty() || !scheme)
    return;
  const auto resolved = resolveAppearanceTheme(
      m_installedThemes, {.themeId = requested, .colorScheme = *scheme},
      m_platformScheme);
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

} // namespace QindaQt::AppAppearance

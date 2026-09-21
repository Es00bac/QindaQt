// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/lock_screen_saver_store.h>

#include <KConfig>
#include <KConfigGroup>

namespace QindaQt::Apps::SettingsScreensaver {

namespace {

constexpr auto kGreeterGroup = "Greeter";
constexpr auto kWallpaperGroup = "Wallpaper";
constexpr auto kGeneralGroup = "General";
constexpr auto kWallpaperPluginKey = "WallpaperPlugin";
constexpr auto kSaverKey = "Saver";
// Where the plugin the greeter had before QindaQt took it over is remembered,
// so turning the screensaver off gives the user their own lock wallpaper back
// instead of leaving them on a plain ground.
constexpr auto kPreviousPluginKey = "PreviousPlugin";
constexpr auto kNoneToken = "none";
// ADR-0226: "blank" is an enabling token here -- the plugin paints its dark
// ground for it -- so only "none" (or an empty token) hands the greeter back
// its previous wallpaper.

[[nodiscard]] KConfigGroup pluginGroup(KConfig &config) {
  return config.group(QString::fromLatin1(kGreeterGroup))
      .group(QString::fromLatin1(kWallpaperGroup))
      .group(KConfigLockScreenSaverStore::wallpaperPluginId())
      .group(QString::fromLatin1(kGeneralGroup));
}

} // namespace

const QString &KConfigLockScreenSaverStore::wallpaperPluginId() {
  static const QString id = QStringLiteral("studio.qinda.screensaver");
  return id;
}

KConfigLockScreenSaverStore::KConfigLockScreenSaverStore(QString filePath)
    : m_filePath(std::move(filePath)) {}

QString KConfigLockScreenSaverStore::currentSaver() const {
  KConfig config(m_filePath, KConfig::SimpleConfig);
  const KConfigGroup greeter = config.group(QString::fromLatin1(kGreeterGroup));
  if (greeter.readEntry(QString::fromLatin1(kWallpaperPluginKey), QString{})
      != wallpaperPluginId()) {
    // Someone else owns the lock wallpaper; the saver is not on screen there.
    return QString::fromLatin1(kNoneToken);
  }
  return pluginGroup(config).readEntry(QString::fromLatin1(kSaverKey),
                                       QString::fromLatin1(kNoneToken));
}

bool KConfigLockScreenSaverStore::save(const QString &saverToken, QString *error) {
  if (m_filePath.trimmed().isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("the screen locker configuration location is unavailable");
    }
    return false;
  }

  KConfig config(m_filePath, KConfig::SimpleConfig);
  KConfigGroup greeter = config.group(QString::fromLatin1(kGreeterGroup));
  KConfigGroup plugin = pluginGroup(config);
  const QString installed =
      greeter.readEntry(QString::fromLatin1(kWallpaperPluginKey), QString{});
  const bool enabling = !saverToken.isEmpty()
      && saverToken != QLatin1String(kNoneToken);

  if (enabling) {
    plugin.writeEntry(QString::fromLatin1(kSaverKey), saverToken);
    if (installed != wallpaperPluginId()) {
      // Remember only the first displaced plugin: taking over twice must not
      // overwrite the user's own choice with our own id.
      plugin.writeEntry(QString::fromLatin1(kPreviousPluginKey), installed);
      greeter.writeEntry(QString::fromLatin1(kWallpaperPluginKey), wallpaperPluginId());
    }
  } else {
    plugin.writeEntry(QString::fromLatin1(kSaverKey), QString::fromLatin1(kNoneToken));
    if (installed == wallpaperPluginId()) {
      const QString previous =
          plugin.readEntry(QString::fromLatin1(kPreviousPluginKey), QString{});
      if (previous.isEmpty()) {
        // No recorded predecessor: leave the key absent so the greeter falls
        // back to its own default rather than to a plugin we invented.
        greeter.deleteEntry(QString::fromLatin1(kWallpaperPluginKey));
      } else {
        greeter.writeEntry(QString::fromLatin1(kWallpaperPluginKey), previous);
      }
      plugin.deleteEntry(QString::fromLatin1(kPreviousPluginKey));
    }
  }

  if (!config.sync()) {
    if (error != nullptr) {
      *error = QStringLiteral("the screen locker configuration could not be written");
    }
    return false;
  }
  if (error != nullptr) {
    error->clear();
  }
  return true;
}

} // namespace QindaQt::Apps::SettingsScreensaver

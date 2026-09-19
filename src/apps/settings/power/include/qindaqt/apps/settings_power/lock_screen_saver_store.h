// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Apps::SettingsPower {

// Boundary for the one fact a locked session needs: which screensaver its
// greeter should draw (ADR-0216). KScreenLocker's greeter renders a KPackage
// wallpaper plugin, so "the saver runs while locked" is a greeter
// configuration, not a process QindaQt starts above the lock screen.
//
// AGENT-GUARD: the write set is exactly `[Greeter] WallpaperPlugin` and the
// `Saver`/`PreviousPlugin` pair inside this plugin's own configuration group.
// The `[Daemon]` group belongs to ScreenLockPreferencesStore and must not be
// touched here: automatic locking is a separate preference with a separate
// section in Settings, and one store writing both would let a screensaver
// choice silently change when the session locks.
class LockScreenSaverStore {
public:
  virtual ~LockScreenSaverStore() = default;
  // `saverToken` is the persisted `power.screensaver` token. The reserved
  // "none" hands the greeter back whatever wallpaper plugin it had before.
  [[nodiscard]] virtual bool save(const QString &saverToken, QString *error) = 0;
  [[nodiscard]] virtual QString currentSaver() const = 0;
};

class KConfigLockScreenSaverStore final : public LockScreenSaverStore {
public:
  explicit KConfigLockScreenSaverStore(QString filePath);

  [[nodiscard]] bool save(const QString &saverToken, QString *error) override;
  [[nodiscard]] QString currentSaver() const override;

  // The KPackage id installed from `data/lockscreen/`.
  [[nodiscard]] static const QString &wallpaperPluginId();

private:
  QString m_filePath;
};

} // namespace QindaQt::Apps::SettingsPower

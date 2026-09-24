// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>

#include <QDBusConnection>
#include <QString>

#include <optional>

namespace QindaQt::Apps::SettingsScreensaver {

class ProcessScreensaverPreview::ScreensaverPreviewIdleLockInhibitor final {
public:
  ScreensaverPreviewIdleLockInhibitor() = default;
  ~ScreensaverPreviewIdleLockInhibitor();

  ScreensaverPreviewIdleLockInhibitor(
      const ScreensaverPreviewIdleLockInhibitor &) = delete;
  ScreensaverPreviewIdleLockInhibitor &
  operator=(const ScreensaverPreviewIdleLockInhibitor &) = delete;

  [[nodiscard]] bool acquire(QString *error);
  void release();

private:
  void disconnectBus();

  QString m_connectionName;
  std::optional<QDBusConnection> m_bus;
  std::optional<uint> m_cookie;
};

} // namespace QindaQt::Apps::SettingsScreensaver

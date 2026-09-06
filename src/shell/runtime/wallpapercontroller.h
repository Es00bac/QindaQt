// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QObject>
#include <QStringList>
class QGuiApplication;
class QQmlEngine;
class QQuickWindow;
class QScreen;
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}
namespace QindaQt::Shell {
class WallpaperController final : public QObject {
  Q_OBJECT
public:
  WallpaperController(QGuiApplication &app, QQmlEngine &engine,
                      Services::SettingsClient::SettingsClient &settings,
                      QStringList dataRoots, QObject *parent = nullptr);
  ~WallpaperController() override;
  void start();

private:
  void applySnapshot();
  void reconcile();
  void createWindow(QScreen *screen);
  QGuiApplication &m_app;
  QQmlEngine &m_engine;
  Services::SettingsClient::SettingsClient &m_settings;
  QStringList m_dataRoots;
  QHash<QScreen *, QQuickWindow *> m_windows;
  QString m_source;
  QString m_mode{QStringLiteral("scaled")};
};
} // namespace QindaQt::Shell

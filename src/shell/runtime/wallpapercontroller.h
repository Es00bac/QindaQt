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
class ShortcutNoteController;
class WallpaperController final : public QObject {
  Q_OBJECT
public:
  WallpaperController(QGuiApplication &app, QQmlEngine &engine,
                      Services::SettingsClient::SettingsClient &settings,
                      QStringList dataRoots, QObject *parent = nullptr);
  ~WallpaperController() override;
  void start();

  // Lends the desktop shortcut-note controller (ADR-0084) its presentation
  // slot: every per-output background window created afterwards hosts one
  // note card. The note controller is QObject-parented to this controller and
  // must outlive `start()`; ownership is not transferred here.
  void setShortcutNote(ShortcutNoteController *note);

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
  ShortcutNoteController *m_shortcutNote = nullptr;
};
} // namespace QindaQt::Shell

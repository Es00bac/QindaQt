// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
class QGuiApplication;
class QQmlEngine;
class QQuickWindow;
class QScreen;
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}
namespace QindaQt::Shell {
class ShortcutNoteController;
class WallpaperSelectionSource;
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
  // The lent note (may be null); the desktop menu's Help toggles it.
  [[nodiscard]] ShortcutNoteController *shortcutNote() const noexcept { return m_shortcutNote; }

  // ADR-0286: per-display and per-desktop choices. Borrowed and guarded; with
  // no source (or a source that knows nothing yet) every output shows the
  // everywhere wallpaper, exactly as before per-output choices existed.
  void setSelectionSource(WallpaperSelectionSource *source);
  // The theme's motion token in milliseconds. A changed picture cross-fades
  // over this long; reduced motion (accessibility.reducedMotion) swaps it
  // instantly instead.
  void setMotionDuration(int milliseconds);

private:
  void applySnapshot();
  void reconcile();
  void refresh();
  void createWindow(QScreen *screen);
  [[nodiscard]] QUrl sourceFor(const QScreen &screen) const;
  [[nodiscard]] int transitionMilliseconds() const noexcept;
  QGuiApplication &m_app;
  QQmlEngine &m_engine;
  Services::SettingsClient::SettingsClient &m_settings;
  QStringList m_dataRoots;
  QHash<QScreen *, QQuickWindow *> m_windows;
  // The confirmed `appearance.wallpaper` preference, unresolved: each output
  // resolves its own choice before turning it into a file.
  QString m_everywhere;
  QString m_mode{QStringLiteral("scaled")};
  // AGENT-GUARD: starts true so nothing animates before the first confirmed
  // snapshot says the user allows motion.
  bool m_reducedMotion = true;
  int m_motionMilliseconds = 0;
  ShortcutNoteController *m_shortcutNote = nullptr;
  QPointer<WallpaperSelectionSource> m_selection;
};
} // namespace QindaQt::Shell

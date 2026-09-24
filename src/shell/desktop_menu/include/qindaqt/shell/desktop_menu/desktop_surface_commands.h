// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

namespace QindaQt::Shell::DesktopMenu {

// The desktop menu's channel to the live desktop-icons surfaces (ADR-0260).
// The surfaces are per-output QML windows that own their icon selection and
// their DesktopContentsController/NewFolderController instances, so the shell
// cannot call those directly; it asks through this object instead and each
// surface runs the request through the SAME QML path its right-click menu
// uses (DesktopContextMenu.dispatch / DesktopIconsView.selectAll).
//
// AGENT-CONTRACT (both sides): DesktopSurface.qml reaches this object as
// `access.desktopCommands` (DesktopControlsAccess), calls
// attachSurface(screenName, primary) when it is created and whenever it
// becomes or stops being the primary output's surface, detachSurface() when
// destroyed, and reportPasteAvailable() from the primary surface whenever its
// contents controller's `canPaste` changes. It handles commandRequested():
// a `primaryOnly` command runs on the primary surface alone (one New Folder,
// one paste, one clean-up), the rest on every surface. The shell side calls
// request(), which refuses while no primary surface is attached.
//
// GUI-thread only; owned by shell composition and borrowed by the desktop
// menu targets and the desktop controls access facade.
class DesktopSurfaceCommands final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool surfaceAttached READ surfaceAttached NOTIFY stateChanged)
  Q_PROPERTY(bool pasteAvailable READ pasteAvailable NOTIFY stateChanged)

public:
  enum class Command {
    NewFolder,
    Paste,
    SelectAll,
    CleanUp,
  };

  explicit DesktopSurfaceCommands(QObject *parent = nullptr);

  // True while a primary desktop-icons surface is attached.
  [[nodiscard]] bool surfaceAttached() const noexcept;
  // The primary surface's last `canPaste` report; false without one.
  [[nodiscard]] bool pasteAvailable() const noexcept;

  // Emits commandRequested once and returns true, or returns false without
  // emitting when no primary surface is attached.
  bool request(Command command);

  Q_INVOKABLE void attachSurface(const QString &screenName, bool primary);
  Q_INVOKABLE void detachSurface(const QString &screenName);
  Q_INVOKABLE void reportPasteAvailable(const QString &screenName, bool available);

Q_SIGNALS:
  // `command` is "new-folder", "paste", "select-all", or "clean-up".
  void commandRequested(QString command, bool primaryOnly);
  void stateChanged();

private:
  [[nodiscard]] QString primaryScreen() const;

  // Attached surfaces by screen name; the value says whether it is primary.
  QHash<QString, bool> m_surfaces;
  QString m_pasteReporter;
  bool m_pasteReported = false;
};

} // namespace QindaQt::Shell::DesktopMenu

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Shell::DesktopControls {

// The single object the panel factory hands to panel QML for the desktop
// controls. It carries only purpose-specific applet facades (each already a
// least-authority projection) and never a client, transport, or bus handle.
// BuiltinAppletContent passes exactly one sub-facade to each applet
// component; the root is not exposed to any applet.
//
// AGENT-CONTRACT: every pointer is borrowed and may be null (grant denied or
// collaborator absent). Shell composition owns the lifetimes and destroys the
// panel windows before the facades.
class DesktopControlsAccess final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *workspaces READ workspaces CONSTANT)
  Q_PROPERTY(QObject *workspaceSwitcher READ workspaceSwitcher CONSTANT)
  Q_PROPERTY(QObject *workspaceTiles READ workspaceTiles CONSTANT)
  Q_PROPERTY(QObject *showDesktop READ showDesktop CONSTANT)
  Q_PROPERTY(QObject *applicationTiles READ applicationTiles CONSTANT)
  Q_PROPERTY(QObject *dashboard READ dashboard CONSTANT)
  Q_PROPERTY(QObject *systemStatus READ systemStatus CONSTANT)
  Q_PROPERTY(QObject *systemMenu READ systemMenu CONSTANT)
  Q_PROPERTY(QObject *places READ places CONSTANT)
  Q_PROPERTY(QObject *quickLaunch READ quickLaunch CONSTANT)
  Q_PROPERTY(QObject *activeApplication READ activeApplication CONSTANT)
  Q_PROPERTY(QObject *commandPalette READ commandPalette CONSTANT)
  Q_PROPERTY(QObject *commandHud READ commandHud CONSTANT)
  Q_PROPERTY(QObject *overview READ overview CONSTANT)
  Q_PROPERTY(QObject *launcher READ launcher CONSTANT)

public:
  struct Facades {
    QObject *workspaces = nullptr;
    QObject *workspaceSwitcher = nullptr;
    QObject *workspaceTiles = nullptr;
    QObject *showDesktop = nullptr;
    QObject *applicationTiles = nullptr;
    QObject *dashboard = nullptr;
    QObject *systemStatus = nullptr;
    QObject *systemMenu = nullptr;
    QObject *places = nullptr;
    QObject *quickLaunch = nullptr;
    QObject *activeApplication = nullptr;
    QObject *commandPalette = nullptr;
    QObject *commandHud = nullptr;
    QObject *overview = nullptr;
    QObject *launcher = nullptr;
  };

  explicit DesktopControlsAccess(Facades facades, QObject *parent = nullptr);

  [[nodiscard]] QObject *workspaces() const noexcept { return m_facades.workspaces; }
  [[nodiscard]] QObject *workspaceSwitcher() const noexcept
  {
    return m_facades.workspaceSwitcher != nullptr ? m_facades.workspaceSwitcher
                                                   : m_facades.workspaces;
  }
  [[nodiscard]] QObject *workspaceTiles() const noexcept
  {
    return m_facades.workspaceTiles != nullptr ? m_facades.workspaceTiles
                                               : m_facades.workspaces;
  }
  [[nodiscard]] QObject *showDesktop() const noexcept
  {
    return m_facades.showDesktop != nullptr ? m_facades.showDesktop : m_facades.workspaces;
  }
  [[nodiscard]] QObject *applicationTiles() const noexcept
  {
    return m_facades.applicationTiles != nullptr ? m_facades.applicationTiles
                                                 : m_facades.quickLaunch;
  }
  [[nodiscard]] QObject *dashboard() const noexcept
  {
    return m_facades.dashboard;
  }
  [[nodiscard]] QObject *systemStatus() const noexcept { return m_facades.systemStatus; }
  [[nodiscard]] QObject *systemMenu() const noexcept { return m_facades.systemMenu; }
  [[nodiscard]] QObject *places() const noexcept { return m_facades.places; }
  [[nodiscard]] QObject *quickLaunch() const noexcept { return m_facades.quickLaunch; }
  [[nodiscard]] QObject *activeApplication() const noexcept
  {
    return m_facades.activeApplication;
  }
  [[nodiscard]] QObject *commandPalette() const noexcept { return m_facades.commandPalette; }
  [[nodiscard]] QObject *commandHud() const noexcept { return m_facades.commandHud; }
  [[nodiscard]] QObject *overview() const noexcept { return m_facades.overview; }
  [[nodiscard]] QObject *launcher() const noexcept { return m_facades.launcher; }

private:
  Facades m_facades;
};

} // namespace QindaQt::Shell::DesktopControls

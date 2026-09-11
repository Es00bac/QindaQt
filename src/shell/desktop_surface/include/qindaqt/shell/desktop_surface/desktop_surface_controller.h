// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QVariantList>

class QGuiApplication;
class QQmlEngine;
class QQuickWindow;
class QScreen;

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}
namespace QindaQt::Applets {
class ManifestCatalog;
}
namespace QindaQt::Profiles {
class LayoutProfile;
}

namespace QindaQt::Shell::DesktopSurface {

// Per-output background-layer surface hosting the desktop-icons applet
// (ADR-0125): places as selectable icon tiles, a styled right-click context
// menu, and a modifier-gated Applications popup. Strictly additive: a profile
// whose resolved desktop inventory is empty produces zero windows, so
// profiles without a `desktop` section behave exactly as before.
//
// AGENT-CONTRACT: both facades are borrowed, may be null (fail-closed
// grants), and must outlive this controller; the QML root receives them as
// plain QObject* and never downcasts. ShellRuntimeApplication::resetRuntime()
// destroys this controller before either borrowed facade.
class DesktopSurfaceController final : public QObject {
  Q_OBJECT
public:
  struct BorrowedFacades {
    QObject *desktopControlsAccess = nullptr;
    QObject *launcherAccess = nullptr;
  };

  DesktopSurfaceController(QGuiApplication &app, QQmlEngine &engine,
                           BorrowedFacades facades, QObject *parent = nullptr);
  ~DesktopSurfaceController() override;

  // Re-resolves profile.desktopApplets against the manifest catalog and
  // capability policy (the same truth panel resolution uses) and reconciles
  // the per-screen window set. Live windows re-read the published `applets`
  // inventory in place, so layout adoption updates the desktop without
  // recreating its windows.
  void adoptProfile(const Profiles::LayoutProfile &profile,
                    const Applets::ManifestCatalog &applets,
                    const AppletHost::CapabilityPolicy &policy);

  // Creates one window per current screen. ShellRuntimeApplication must call
  // this only after the wallpaper controller has shown its windows so this
  // surface maps above them (see the AGENT-NOTE in createWindow).
  // AGENT-GUARD: adoption before start() only re-resolves the inventory —
  // window creation begins with the first start() call, so a session can
  // adopt and measure profiles without mapping any surface.
  void start();

  [[nodiscard]] const QVariantList &inventory() const noexcept
  {
    return m_inventory;
  }
  [[nodiscard]] int windowCount() const noexcept
  {
    return static_cast<int>(m_windows.size());
  }

private:
  void reconcile();
  void createWindow(QScreen *screen);

  QGuiApplication &m_app;
  QQmlEngine &m_engine;
  BorrowedFacades m_facades;
  QVariantList m_inventory;
  QHash<QScreen *, QQuickWindow *> m_windows;
  bool m_started = false;
};

} // namespace QindaQt::Shell::DesktopSurface

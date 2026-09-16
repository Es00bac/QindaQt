// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QVariantList>

#include <memory>

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

class DesktopIconLayoutStore;

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
//
// AGENT-CONTRACT (ADR-0167): the desktop is one desktop spread over several
// output surfaces, not one desktop per output. This controller owns the single
// DesktopIconLayoutStore and injects the same object, plus the global output
// geometry, into every surface. Each surface then draws only the icons its own
// output owns. Never let a surface construct its own store.
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

  // Test seam: the store every surface shares. Never null.
  [[nodiscard]] DesktopIconLayoutStore &layoutStore() const noexcept
  {
    return *m_layoutStore;
  }

private:
  void reconcile();
  void createWindow(QScreen *screen);
  // Global-frame {name, x, y, width, height} for every connected output.
  [[nodiscard]] QVariantList outputRects() const;
  [[nodiscard]] QString primaryOutputName() const;
  // Republishes output geometry to live windows and rewatches every screen.
  void refreshOutputs();

  QGuiApplication &m_app;
  QQmlEngine &m_engine;
  BorrowedFacades m_facades;
  QVariantList m_inventory;
  QHash<QScreen *, QQuickWindow *> m_windows;
  std::unique_ptr<DesktopIconLayoutStore> m_layoutStore;
  QList<QMetaObject::Connection> m_screenConnections;
  bool m_started = false;
};

} // namespace QindaQt::Shell::DesktopSurface

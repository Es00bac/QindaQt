// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QMargins>
#include <QMetaObject>
#include <QObject>
#include <QString>
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

  // Borrowed live customization facade (Meta+right-click desktop menu and
  // edit mode). May be null; the QML disables its entries. Set before or
  // after start(): live windows receive it in place.
  void setCustomizationAccess(QObject *access);

  // Depth, in logical pixels from each edge, that the shell's panels reserve
  // on each output, keyed by QScreen::name(). Each surface's icons flow and
  // clamp inside its output minus these insets - the desktop work area -
  // while the surface itself keeps spanning the whole output. An output with
  // no entry is all work area. Set before or after start(): live windows are
  // republished in place, and an unchanged map is ignored.
  //
  // AGENT-CONTRACT (ADR-0261): ShellRuntimeApplication::reconcileSurfaces()
  // is the only producer; it publishes PanelReservationInsets::fromPlan() of
  // every accepted panel plan. This is the whole interface between panels and
  // the desktop surface: it never reads panel windows, profiles, or
  // QScreen::availableGeometry(), which layer-shell exclusive zones do not
  // reach.
  void setOutputReservations(const QHash<QString, QMargins> &reservations);
  [[nodiscard]] const QHash<QString, QMargins> &outputReservations() const noexcept
  {
    return m_reservations;
  }

  // What every surface receives as `outputRects`: one global-frame
  // {name, x, y, width, height, workArea: {x, y, width, height}} map per
  // connected output. The work area fails open to the whole output when the
  // insets would leave no room at all.
  [[nodiscard]] QVariantList outputRects() const;

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
  [[nodiscard]] QString primaryOutputName() const;
  // Pushes outputRects() and the primary name onto every live window.
  void publishGeometry();
  // Republishes output geometry to live windows and rewatches every screen.
  void refreshOutputs();

  QGuiApplication &m_app;
  QQmlEngine &m_engine;
  BorrowedFacades m_facades;
  QObject *m_customizationAccess = nullptr;
  QVariantList m_inventory;
  QHash<QScreen *, QQuickWindow *> m_windows;
  QHash<QString, QMargins> m_reservations;
  std::unique_ptr<DesktopIconLayoutStore> m_layoutStore;
  QList<QMetaObject::Connection> m_screenConnections;
  bool m_started = false;
};

} // namespace QindaQt::Shell::DesktopSurface

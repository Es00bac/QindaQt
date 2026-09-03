// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_shell/menu_export/window_menu_identity.h>

#include <QGuiApplication>
#include <QWindow>
#include <QtGui/private/qdesktopunixservices_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qguiapplication_platform.h>
#include <QtGui/qpa/qplatformintegration.h>
#include <QtGui/qpa/qplatformwindow_p.h>

#include <limits>

namespace QindaQt::AppShell::MenuExport {
namespace {

class QtWindowMenuIdentityPublisher final : public WindowMenuIdentityPublisher {
public:
  std::optional<WindowMenuIdentity>
  publish(QWindow &window, const QString &serviceName,
          const QString &objectPath) override {
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("xcb")) {
      const WId nativeId = window.winId();
      if (nativeId == 0 || nativeId > std::numeric_limits<quint32>::max()) {
        return std::nullopt;
      }
      return WindowMenuIdentity{.kind = WindowMenuIdentityKind::XWindow,
                                .registrarWindowId =
                                    static_cast<quint32>(nativeId)};
    }
    if (!platform.startsWith(QStringLiteral("wayland"))) {
      return std::nullopt;
    }
    const auto *nativeWindow =
        window.nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    if (nativeWindow == nullptr || nativeWindow->surface() == nullptr) {
      return std::nullopt;
    }
    auto *services = dynamic_cast<QDesktopUnixServices *>(
        QGuiApplicationPrivate::platformIntegration()->services());
    if (services == nullptr) {
      return std::nullopt;
    }
    // AGENT-CONTRACT: Qt's Wayland services owns the KDE appmenu protocol
    // object tied to this wl_surface. This is the service/path pair KWin
    // projects through CompositorShell1; no numeric Wayland ID is valid.
    services->registerDBusMenuForWindow(&window, serviceName, objectPath);
    m_waylandPublished = true;
    return WindowMenuIdentity{.kind =
                                  WindowMenuIdentityKind::WaylandAnnouncement,
                              .registrarWindowId = std::nullopt};
  }

  void withdraw(QWindow &window) override {
    if (!m_waylandPublished) {
      return;
    }
    if (auto *services = dynamic_cast<QDesktopUnixServices *>(
            QGuiApplicationPrivate::platformIntegration()->services())) {
      services->unregisterDBusMenuForWindow(&window);
    }
    m_waylandPublished = false;
  }

private:
  bool m_waylandPublished = false;
};

} // namespace

std::unique_ptr<WindowMenuIdentityPublisher>
createQtWindowMenuIdentityPublisher() {
  return std::make_unique<QtWindowMenuIdentityPublisher>();
}

} // namespace QindaQt::AppShell::MenuExport

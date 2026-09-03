// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QtGlobal>

#include <memory>
#include <optional>

class QWindow;

namespace QindaQt::AppShell::MenuExport {

enum class WindowMenuIdentityKind {
  XWindow,
  WaylandAnnouncement,
};

struct WindowMenuIdentity final {
  WindowMenuIdentityKind kind = WindowMenuIdentityKind::XWindow;
  std::optional<quint32> registrarWindowId;
};

// Injected platform boundary for associating one application-owned QWindow
// with its dbusmenu endpoint. Implementations must return an X window ID only
// when it is the real native identifier, or publish the service/path through
// the native Wayland appmenu protocol. They never invent a Wayland number.
class WindowMenuIdentityPublisher {
public:
  virtual ~WindowMenuIdentityPublisher() = default;

  [[nodiscard]] virtual std::optional<WindowMenuIdentity>
  publish(QWindow &window, const QString &serviceName,
          const QString &objectPath) = 0;
  virtual void withdraw(QWindow &window) = 0;
};

// Creates the Qt platform implementation. The returned publisher owns no
// window and must be destroyed before the QGuiApplication.
[[nodiscard]] std::unique_ptr<WindowMenuIdentityPublisher>
createQtWindowMenuIdentityPublisher();

} // namespace QindaQt::AppShell::MenuExport

// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/app_shell/menu_export/window_menu_identity.h>

#include <QDBusConnection>
#include <QObject>

#include <memory>
#include <optional>

class QWindow;

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::AppShell::MenuExport {

enum class MenuExportStatus {
  Disabled,
  WaitingForRegistrar,
  Registering,
  Published,
};

// Opt-in application-side composition for one AppShell primary window. The
// coordinator, window, injected bus handle, and identity publisher are all
// constructor-visible. The object owns its dbusmenu endpoint and async calls;
// it executes no blocking D-Bus request and must live on the GUI thread.
// `Published` proves endpoint publication plus registrar registration (X11)
// or platform announcement (Wayland), never that the shell is displaying it.
class ApplicationMenuExport final : public QObject {
  Q_OBJECT

public:
  ApplicationMenuExport(
      ApplicationCoordinator &coordinator, QWindow &window,
      QDBusConnection sessionBus,
      std::unique_ptr<WindowMenuIdentityPublisher> identityPublisher,
      QObject *parent = nullptr);
  ~ApplicationMenuExport() override;

  ApplicationMenuExport(const ApplicationMenuExport &) = delete;
  ApplicationMenuExport &operator=(const ApplicationMenuExport &) = delete;

  [[nodiscard]] static std::unique_ptr<ApplicationMenuExport>
  compose(ApplicationCoordinator &coordinator, QWindow &window,
          QDBusConnection sessionBus);

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] MenuExportStatus status() const noexcept;
  [[nodiscard]] bool published() const noexcept;
  [[nodiscard]] std::optional<quint32> registeredWindowId() const noexcept;
  [[nodiscard]] QString failureCode() const;

Q_SIGNALS:
  void statusChanged();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::AppShell::MenuExport

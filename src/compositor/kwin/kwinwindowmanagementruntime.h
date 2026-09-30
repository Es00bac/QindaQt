// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/window_management/controller.h>
namespace QindaQt::Compositor::KWinIntegration {
class ManagedWindowRegistry;
class KWinHybridSession;
// Borrows one live compositor graph. Native Window objects never escape this
// adapter; capture/resolution/dispatch run synchronously on the GUI thread.
class KWinWindowManagementRuntime final : public WindowManagement::Scene,
                                          public WindowManagement::Executor {
public:
  KWinWindowManagementRuntime(
      ManagedWindowRegistry &registry, KWinHybridSession &session,
      std::function<std::optional<qint64>()> providerPid);
  std::optional<WindowManagement::ContextSnapshot> capture() const override;
  bool
  current(const WindowManagement::ContextSnapshot &snapshot) const override;
  WindowManagement::Resolution
  resolve(const WindowManagement::Target &target,
          const WindowManagement::ContextSnapshot &snapshot) const override;
  QStringList capabilities() const override;
  WindowManagement::Result
  execute(const WindowManagement::Command &command,
          const WindowManagement::ResolvedTarget &target) override;

private:
  QByteArray fingerprint(const QString &windowId) const;
  ManagedWindowRegistry &m_registry;
  KWinHybridSession &m_session;
  std::function<std::optional<qint64>()> m_providerPid;
};
} // namespace QindaQt::Compositor::KWinIntegration

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <memory>
namespace QindaQt::Compositor::KWinIntegration {
class ManagedWindowRegistry;
class KWinHybridSession;
// Owns transport, confirmed Voice1 admission and platform adapters for one
// compositor lifetime. Destroy before the borrowed registry/Hybrid session.
class KWinWindowManagementService final : public QObject {
public:
  KWinWindowManagementService(ManagedWindowRegistry &registry,
                              KWinHybridSession &session,
                              QDBusConnection connection,
                              QObject *parent = nullptr);
  ~KWinWindowManagementService() override;
  [[nodiscard]] bool start();

private:
  class Private;
  std::unique_ptr<Private> d;
};
} // namespace QindaQt::Compositor::KWinIntegration

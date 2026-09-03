// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QString>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::ShellWindowActionsClient {
class ShellWindowActionsClient;
}

namespace QindaQt::Shell::GlobalMenu {
class GlobalMenuAppletAccess;
namespace Composition {
class GlobalMenuTransportCoordinator;
}
namespace Registrar {
class AppMenuRegistrar;
}
}

namespace QindaQt::Shell {

class GlobalMenuIdentityAdapter;

enum class GlobalMenuRuntimeStatus {
    Ready,
    Degraded,
    Unavailable,
};

// Shell-private ownership boundary for registrar residency and the G1
// transport coordinator. It borrows the shell's one exact-owner compositor
// client; no second compositor connection is created here.
class GlobalMenuAppletComposition final
{
public:
    GlobalMenuAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        QDBusConnection sessionBus,
        ShellWindowActionsClient::ShellWindowActionsClient &windowActions);
    ~GlobalMenuAppletComposition();

    GlobalMenuAppletComposition(const GlobalMenuAppletComposition &) = delete;
    GlobalMenuAppletComposition &operator=(const GlobalMenuAppletComposition &) = delete;

    void start();
    void stop();
    [[nodiscard]] GlobalMenu::GlobalMenuAppletAccess *access() const noexcept;
    [[nodiscard]] GlobalMenuRuntimeStatus status() const noexcept;
    [[nodiscard]] const QString &reasonCode() const noexcept;

private:
    QDBusConnection m_sessionBus;
    ShellWindowActionsClient::ShellWindowActionsClient &m_windowActions;
    bool m_granted = false;
    GlobalMenuRuntimeStatus m_status = GlobalMenuRuntimeStatus::Unavailable;
    QString m_reasonCode;
    std::unique_ptr<GlobalMenu::GlobalMenuAppletAccess> m_access;
    std::unique_ptr<GlobalMenuIdentityAdapter> m_identity;
    std::unique_ptr<GlobalMenu::Registrar::AppMenuRegistrar> m_registrar;
    std::unique_ptr<GlobalMenu::Composition::GlobalMenuTransportCoordinator>
        m_coordinator;
};

} // namespace QindaQt::Shell

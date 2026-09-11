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

namespace QindaQt::Profiles {
class LayoutProfile;
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
    // AGENT-CONTRACT (ADR-0130): the registrar name is a session-wide signal
    // that a menu host exists; Qt's platform theme hides each new QMenuBar
    // while it has an owner. The shell therefore owns it only while the
    // adopted layout resolves a ready global-menu instance, using the same
    // resolution as the panel dispatcher and desktop surface.
    [[nodiscard]] static bool layoutHostsGlobalMenu(
        const Profiles::LayoutProfile &profile,
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy);
    // Idempotent residency transition for startup and every live layout
    // adoption: starts unless already Ready when hosted; otherwise stops,
    // releasing the name, with reason `global-menu-not-hosted`.
    void followLayout(bool hosted);
    [[nodiscard]] bool registrarResident() const noexcept;
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

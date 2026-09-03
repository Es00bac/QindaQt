// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <plugin.h>

#include <QDBusConnection>

#include <memory>

namespace QindaQt::Compositor {
class ContainerControlBridge;
class ShellWindowActionController;
}

namespace QindaQt::Compositor::KWinIntegration {

class KWinControlEndpoint;
class KWinDevelopmentInputInjector;
class KWinDevelopmentOutputSeam;
class KWinInputAdapter;
class KWinOutputInventory;
class KWinHybridSession;
class KWinSceneAdapter;
class KWinShellVisibilityPublisher;
class KWinShellPanelOwnerSource;
class KWinShellWindowRegistry;
class KWinShellWindowActionExecutor;
class KWinShellWindowActionsEndpoint;
class QtBusShellCredentialSource;
class ManagedWindowRegistry;

class QindaQtKWinPlugin final : public KWin::Plugin
{
    Q_OBJECT

public:
    QindaQtKWinPlugin();
    ~QindaQtKWinPlugin() override;

private Q_SLOTS:
    void reconcileClosedWindow(const QString &windowId, const QString &containerId);

private:
    void releasePublishedContainers();

    const bool m_mutationsEnabled;
    const bool m_developmentVirtualOutputsEnabled;
    QDBusConnection m_bus;
    std::unique_ptr<ManagedWindowRegistry> m_registry;
    std::unique_ptr<KWinOutputInventory> m_outputInventory;
    std::unique_ptr<KWinShellVisibilityPublisher> m_shellVisibility;
    std::unique_ptr<QtBusShellCredentialSource> m_shellCredentials;
    std::unique_ptr<KWinShellPanelOwnerSource> m_shellPanelOwner;
    std::unique_ptr<KWinShellWindowRegistry> m_shellActionRegistry;
    std::unique_ptr<KWinShellWindowActionExecutor> m_shellActionExecutor;
    std::unique_ptr<ShellWindowActionController> m_shellActionController;
    std::unique_ptr<KWinShellWindowActionsEndpoint> m_shellActionEndpoint;
    std::unique_ptr<KWinHybridSession> m_hybridSession;
    std::unique_ptr<KWinInputAdapter> m_inputAdapter;
    std::unique_ptr<KWinDevelopmentInputInjector> m_developmentInputInjector;
    std::unique_ptr<KWinDevelopmentOutputSeam> m_developmentOutputSeam;
    std::unique_ptr<KWinSceneAdapter> m_sceneAdapter;
    std::unique_ptr<ContainerControlBridge> m_bridge;
    std::unique_ptr<KWinControlEndpoint> m_endpoint;
    bool m_registeredService = false;
    bool m_registeredObject = false;
    bool m_registeredShellActionObject = false;
};

} // namespace QindaQt::Compositor::KWinIntegration

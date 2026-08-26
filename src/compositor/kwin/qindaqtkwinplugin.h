// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <plugin.h>

#include <QDBusConnection>

#include <memory>

namespace QindaQt::Compositor {
class ContainerControlBridge;
}

namespace QindaQt::Compositor::KWinIntegration {

class KWinControlEndpoint;
class KWinDevelopmentInputInjector;
class KWinInputAdapter;
class KWinHybridSession;
class KWinSceneAdapter;
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
    QDBusConnection m_bus;
    std::unique_ptr<ManagedWindowRegistry> m_registry;
    std::unique_ptr<KWinHybridSession> m_hybridSession;
    std::unique_ptr<KWinInputAdapter> m_inputAdapter;
    std::unique_ptr<KWinDevelopmentInputInjector> m_developmentInputInjector;
    std::unique_ptr<KWinSceneAdapter> m_sceneAdapter;
    std::unique_ptr<ContainerControlBridge> m_bridge;
    std::unique_ptr<KWinControlEndpoint> m_endpoint;
    bool m_registeredService = false;
    bool m_registeredObject = false;
};

} // namespace QindaQt::Compositor::KWinIntegration

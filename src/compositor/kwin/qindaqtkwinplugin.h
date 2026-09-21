// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <plugin.h>

namespace KWin {
class InputDevice;
}

#include <QDBusConnection>
#include <QList>
#include <QPointer>

#include <memory>

namespace QindaQt::Compositor {
class ContainerControlBridge;
class ShellWindowActionController;
class ShellWindowIdentityController;
class ShellTaskFactsController;
}

namespace QindaQt::Compositor::KWinIntegration {

class KWinControlEndpoint;
class KWinPointerCornerReserver;
class KWinTouchEdgeReserver;
class KWinTouchPreferences;
class KWinOnScreenKeyboardPolicy;
class PointerCornerGesture;
class TouchEdgeGestures;
class KWinDevelopmentInputInjector;
class KWinDevelopmentOutputSeam;
class KWinInputAdapter;
class KWinOutputInventory;
class KWinHybridSession;
class KWinChromeAppearance;
class KWinSceneAdapter;
class KWinShellVisibilityPublisher;
class KWinShellWindowIdentityPublisher;
class KWinShellTaskFactsPublisher;
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
    void applyTouchPreferences();
    void applyTouchscreenEnabled(bool enabled);
    void gateTouchDevice(KWin::InputDevice *device, bool enabled);

    const bool m_mutationsEnabled;
    const bool m_developmentVirtualOutputsEnabled;
    QDBusConnection m_bus;
    std::unique_ptr<ManagedWindowRegistry> m_registry;
    std::unique_ptr<KWinOutputInventory> m_outputInventory;
    std::unique_ptr<KWinShellVisibilityPublisher> m_shellVisibility;
    std::unique_ptr<KWinChromeAppearance> m_chromeAppearance;
    std::unique_ptr<KWinHybridSession> m_hybridSession;
    std::unique_ptr<KWinShellWindowIdentityPublisher> m_shellIdentity;
    std::unique_ptr<KWinShellTaskFactsPublisher> m_shellTaskFacts;
    std::unique_ptr<QtBusShellCredentialSource> m_shellCredentials;
    std::unique_ptr<KWinShellPanelOwnerSource> m_shellPanelOwner;
    std::unique_ptr<KWinShellWindowRegistry> m_shellActionRegistry;
    std::unique_ptr<KWinShellWindowActionExecutor> m_shellActionExecutor;
    std::unique_ptr<ShellWindowActionController> m_shellActionController;
    std::unique_ptr<ShellWindowIdentityController> m_shellIdentityController;
    std::unique_ptr<ShellTaskFactsController> m_shellTaskFactsController;
    std::unique_ptr<KWinShellWindowActionsEndpoint> m_shellActionEndpoint;
    std::unique_ptr<KWinInputAdapter> m_inputAdapter;
    std::unique_ptr<KWinDevelopmentInputInjector> m_developmentInputInjector;
    std::unique_ptr<KWinDevelopmentOutputSeam> m_developmentOutputSeam;
    std::unique_ptr<KWinSceneAdapter> m_sceneAdapter;
    std::unique_ptr<ContainerControlBridge> m_bridge;
    std::unique_ptr<KWinControlEndpoint> m_endpoint;
    std::unique_ptr<KWinTouchEdgeReserver> m_touchEdgeReserver;
    std::unique_ptr<TouchEdgeGestures> m_touchEdges;
    // ADR-0232: the upper-left corner for a pointer. Separate from the touch
    // edges because KWin's reserveTouch is touch-only and a corner is not one
    // of its four edges.
    std::unique_ptr<KWinPointerCornerReserver> m_pointerCornerReserver;
    std::unique_ptr<PointerCornerGesture> m_pointerCorner;
    std::unique_ptr<KWinTouchPreferences> m_touchPreferences;
    std::unique_ptr<KWinOnScreenKeyboardPolicy> m_onScreenKeyboard;
    // ADR-0205: touch devices this plugin switched off for input.touch.enabled;
    // only these are switched back on, never a device the user disabled in KWin.
    QList<QPointer<KWin::InputDevice>> m_touchDevicesDisabledByPreference;
    bool m_registeredService = false;
    bool m_registeredObject = false;
    bool m_registeredShellActionObject = false;
};

} // namespace QindaQt::Compositor::KWinIntegration

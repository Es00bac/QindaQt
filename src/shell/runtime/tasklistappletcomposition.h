// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QStringList>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::ShellTaskList {
class TaskListSource;
namespace Producer {
class QtTaskListProducerTransport;
class TaskListFactsProducer;
class TaskListOperationAuthority;
}
namespace Operations {
class QtTaskListOperationTransport;
class TaskListOperationAdapter;
}
}

namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
class TaskListAppletOperationBridge;
class TaskListAppletOperationPort;
}

namespace QindaQt::ShellWindowActionsClient {
class ShellWindowActionsClient;
}

namespace QindaQt::Shell::Icons {
class DesktopEntryIconResolver;
class IconThemeLocator;
}

namespace QindaQt::Shell {

class TaskListWindowOperationRouter;

// Shell-private T3 composition. Production owns the T0 source plus T1
// Compositor1 producer/container adapter, while borrowing the shell's sole
// authenticated CompositorShell1 client for window actions. The injected
// constructor keeps private-bus tests on the same public boundaries.
class TaskListAppletComposition final
{
public:
    TaskListAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        const QDBusConnection &sessionBus,
        ShellWindowActionsClient::ShellWindowActionsClient &windowActions,
        QStringList applicationRoots, QStringList iconRoots,
        QStringList iconThemes);
    TaskListAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        ShellTaskList::TaskListSource &source,
        ShellTaskList::Producer::TaskListOperationAuthority &authority,
        ShellTaskListApplet::TaskListAppletOperationPort &containerOperations,
        ShellWindowActionsClient::ShellWindowActionsClient &windowActions);
    ~TaskListAppletComposition();

    TaskListAppletComposition(const TaskListAppletComposition &) = delete;
    TaskListAppletComposition &operator=(const TaskListAppletComposition &) = delete;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    [[nodiscard]] ShellTaskListApplet::TaskListAppletController *access() const noexcept;

private:
    void compose(const Applets::ManifestCatalog &catalog,
                 const AppletHost::CapabilityPolicy &policy,
                 ShellTaskList::TaskListSource &source,
                 ShellTaskList::Producer::TaskListOperationAuthority &authority,
                 ShellTaskListApplet::TaskListAppletOperationPort &containerOperations,
                 ShellWindowActionsClient::ShellWindowActionsClient &windowActions);

    // AGENT-CONTRACT: reverse destruction is controller -> router -> legacy
    // bridge/adapter -> transports/source. Borrowed injected collaborators and
    // the shell-owned window-actions client must outlive this composition;
    // ShellRuntimeApplication destroys panel windows first.
    std::unique_ptr<ShellTaskList::TaskListSource> m_ownedSource;
    std::unique_ptr<ShellTaskList::Producer::QtTaskListProducerTransport>
        m_ownedProducerTransport;
    std::unique_ptr<ShellTaskList::Producer::TaskListFactsProducer>
        m_ownedProducer;
    std::unique_ptr<ShellTaskList::Operations::QtTaskListOperationTransport>
        m_ownedOperationTransport;
    std::unique_ptr<ShellTaskList::Operations::TaskListOperationAdapter>
        m_ownedOperationAdapter;
    std::unique_ptr<ShellTaskListApplet::TaskListAppletOperationBridge>
        m_ownedContainerBridge;
    std::unique_ptr<TaskListWindowOperationRouter> m_router;
    std::unique_ptr<Icons::DesktopEntryIconResolver> m_iconResolver;
    std::unique_ptr<Icons::IconThemeLocator> m_iconThemeLocator;
    std::unique_ptr<ShellTaskListApplet::TaskListAppletController> m_access;
};

} // namespace QindaQt::Shell

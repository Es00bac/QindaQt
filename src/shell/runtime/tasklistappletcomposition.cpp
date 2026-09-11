// SPDX-License-Identifier: GPL-3.0-or-later

#include "tasklistappletcomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/compositor/shellwindowactions.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_operation_bridge.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_operation_port.h"
#include "qindaqt/shell/task_list/operations/qt_task_list_operation_transport.h"
#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"
#include "qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/task_list_source.h"
#include "qindaqt/shell/icons/desktop_entry_icon_resolver.h"
#include "qindaqt/shell/icons/icon_theme_locator.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include <QList>

#include <limits>
#include <optional>
#include <utility>

namespace QindaQt::Shell {
namespace {

using ShellTaskList::Operations::TaskListOperationResult;
using ShellTaskList::Operations::TaskListOperationStatus;

ShellTaskListApplet::TaskListAppletGrants taskListGrants(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    ShellTaskListApplet::TaskListAppletGrants grants;
    const auto *manifest = catalog.findById(QStringLiteral("task-list"));
    if (manifest == nullptr) {
        return grants;
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return grants;
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return grants;
    }
    for (const auto &decision : evaluated.decisions) {
        switch (decision.capability) {
        case Applets::Capability::WindowRead:
            grants.windowsRead = decision.granted();
            break;
        case Applets::Capability::WindowActivate:
            grants.windowsActivate = decision.granted();
            break;
        case Applets::Capability::WindowManage:
            grants.windowsManage = decision.granted();
            break;
        default:
            break;
        }
    }
    return grants;
}

TaskListOperationResult result(quint64 token, TaskListOperationStatus status,
                               QString code, QString message)
{
    return {token, status, std::move(code), std::move(message), 0};
}

Compositor::ShellWindowAction actionFor(
    const ShellTaskList::TaskIntentRequest &request,
    const ShellTaskList::TaskIntentOutcome &outcome)
{
    switch (request.kind) {
    case ShellTaskList::TaskIntentKind::Activate:
        return Compositor::ShellWindowAction::Activate;
    case ShellTaskList::TaskIntentKind::Minimize:
        return outcome.minimized ? Compositor::ShellWindowAction::Unminimize
                                 : Compositor::ShellWindowAction::Minimize;
    case ShellTaskList::TaskIntentKind::Close:
        return Compositor::ShellWindowAction::Close;
    case ShellTaskList::TaskIntentKind::Raise:
        return Compositor::ShellWindowAction::Raise;
    }
    return Compositor::ShellWindowAction::Activate;
}

} // namespace

class TaskListWindowOperationRouter final
    : public ShellTaskListApplet::TaskListAppletOperationPort
{
public:
    TaskListWindowOperationRouter(
        ShellTaskList::TaskListSource &source,
        ShellTaskList::Producer::TaskListOperationAuthority &authority,
        ShellTaskListApplet::TaskListAppletOperationPort &containerOperations,
        ShellWindowActionsClient::ShellWindowActionsClient &windowActions)
        : m_source(source)
        , m_authority(authority)
        , m_containerOperations(containerOperations)
        , m_windowActions(windowActions)
    {
        connect(&m_windowActions,
                &ShellWindowActionsClient::ShellWindowActionsClient::actionFinished,
                this, [this] { finishWindowAction(); });
        connect(&m_containerOperations,
                &ShellTaskListApplet::TaskListAppletOperationPort::operationFinished,
                this, [this](const TaskListOperationResult &operationResult) {
                    finishContainerAction(operationResult);
                });
        m_seenAuthorityOwner = m_authority.uniqueOwner();
        connect(&m_authority,
                &ShellTaskList::Producer::TaskListOperationAuthority::stateChanged,
                this, [this] {
                    const QString currentOwner = m_authority.uniqueOwner();
                    if (currentOwner == m_seenAuthorityOwner) {
                        return;
                    }
                    const bool clearSource = currentOwner.isEmpty()
                        || !m_seenAuthorityOwner.isEmpty();
                    m_seenAuthorityOwner = currentOwner;
                    // AGENT-GUARD: task ids belong to one exact compositor
                    // owner. Degraded last-known-good truth is permitted for
                    // a transient read failure, never across owner lineage.
                    if (clearSource) {
                        m_source.reset();
                        m_source.markDegraded();
                    }
                });
    }

    quint64 executeTaskIntent(
        const ShellTaskList::TaskIntentRequest &request,
        const ShellTaskList::TaskIntentOutcome &outcome) override
    {
        const quint64 token = allocateToken();
        if (token == 0) {
            return 0;
        }
        if (m_pending) {
            emitImmediate(result(token, TaskListOperationStatus::Busy,
                                 QStringLiteral("operation-busy"),
                                 QStringLiteral("another task-list operation is in flight")));
            return token;
        }
        const auto actionGeneration = m_authority.actionGeneration();
        const Compositor::ShellWindowGeneration compositorGeneration =
            actionGeneration
            ? Compositor::ShellWindowGeneration{actionGeneration->epoch,
                                                actionGeneration->revision}
            : Compositor::ShellWindowGeneration{};
        if (!outcome.ok() || m_authority.status()
                != ShellTaskList::TaskListSourceStatus::Ready
            || request.expectedRevision != m_authority.publishedRevision()) {
            emitImmediate(result(token, TaskListOperationStatus::StaleGeneration,
                                 QStringLiteral("stale-generation"),
                                 QStringLiteral("the displayed task generation is no longer current")));
            return token;
        }
        if (m_authority.uniqueOwner().isEmpty()
            || m_authority.uniqueOwner() != m_windowActions.uniqueOwner()
            || !compositorGeneration.isValid()
            || !m_windowActions.available()) {
            emitImmediate(result(token, TaskListOperationStatus::Unavailable,
                                 QStringLiteral("window-actions-unavailable"),
                                 QStringLiteral("authenticated window actions are unavailable")));
            return token;
        }
        if (m_windowActions.requestInFlight()) {
            emitImmediate(result(token, TaskListOperationStatus::Busy,
                                 QStringLiteral("window-actions-busy"),
                                 QStringLiteral("another authenticated window action is in flight")));
            return token;
        }

        QStringList windowIds{outcome.primaryWindowId};
        if (outcome.entryKind == ShellTaskList::TaskEntryKind::Container
            && (request.kind == ShellTaskList::TaskIntentKind::Minimize
                || request.kind == ShellTaskList::TaskIntentKind::Close)) {
            windowIds = outcome.memberWindowIds;
        }
        if (windowIds.isEmpty() || windowIds.contains(QString{})) {
            emitImmediate(result(token, TaskListOperationStatus::Rejected,
                                 QStringLiteral("invalid-window-target"),
                                 QStringLiteral("the task has no actionable window target")));
            return token;
        }
        m_pending = Pending{token, PendingKind::Window, 0, windowIds, 0,
                            actionFor(request, outcome),
                            compositorGeneration};
        requestPendingWindow();
        return token;
    }

    quint64 activateContainerPage(const QString &containerId,
                                  const QString &pageId,
                                  quint64 expectedRevision) override
    {
        return forwardContainer([&] {
            return m_containerOperations.activateContainerPage(
                containerId, pageId, expectedRevision);
        });
    }

    quint64 detachWindow(const QString &containerId, const QString &windowId,
                         quint64 expectedRevision) override
    {
        return forwardContainer([&] {
            return m_containerOperations.detachWindow(
                containerId, windowId, expectedRevision);
        });
    }

    quint64 releaseContainer(const QString &containerId,
                             quint64 expectedRevision) override
    {
        return forwardContainer([&] {
            return m_containerOperations.releaseContainer(containerId,
                                                           expectedRevision);
        });
    }

    quint64 dockWindows(const QString &targetWindowId,
                        const QString &incomingWindowId,
                        const QString &orientation, const QString &position,
                        double ratio, quint64 expectedRevision) override
    {
        return forwardContainer([&] {
            return m_containerOperations.dockWindows(
                targetWindowId, incomingWindowId, orientation, position, ratio,
                expectedRevision);
        });
    }

private:
    enum class PendingKind { Window, Container };
    struct Pending {
        quint64 outerToken = 0;
        PendingKind kind = PendingKind::Window;
        quint64 innerToken = 0;
        QStringList windowIds;
        qsizetype windowIndex = 0;
        Compositor::ShellWindowAction windowAction =
            Compositor::ShellWindowAction::Activate;
        Compositor::ShellWindowGeneration windowGeneration;
    };

    template<typename Dispatch>
    quint64 forwardContainer(Dispatch dispatch)
    {
        const quint64 token = allocateToken();
        if (token == 0) {
            return 0;
        }
        if (m_pending) {
            emitImmediate(result(token, TaskListOperationStatus::Busy,
                                 QStringLiteral("operation-busy"),
                                 QStringLiteral("another task-list operation is in flight")));
            return token;
        }
        Pending pending;
        pending.outerToken = token;
        pending.kind = PendingKind::Container;
        m_pending = std::move(pending);
        m_forwardingContainer = true;
        const quint64 innerToken = dispatch();
        m_forwardingContainer = false;
        if (!m_pending) {
            return token;
        }
        m_pending->innerToken = innerToken;
        drainContainerResults();
        if (innerToken == 0 && m_pending) {
            m_pending.reset();
            emitImmediate(result(token, TaskListOperationStatus::TransportFailure,
                                 QStringLiteral("operation-token-exhausted"),
                                 QStringLiteral("container operation lineage is exhausted")));
        }
        return token;
    }

    void finishWindowAction()
    {
        if (!m_pending || m_pending->kind != PendingKind::Window) {
            return;
        }
        const quint64 token = m_pending->outerToken;
        const auto &clientResult = m_windowActions.lastResult();
        if (!clientResult || clientResult->uncertain || !clientResult->serverResult) {
            m_pending.reset();
            Q_EMIT operationFinished(result(
                token, TaskListOperationStatus::Uncertain,
                clientResult ? clientResult->failureCode
                             : QStringLiteral("window-action-result-missing"),
                clientResult ? clientResult->message
                             : QStringLiteral("the authenticated action result is unavailable")));
            return;
        }
        const auto &server = *clientResult->serverResult;
        switch (server.status) {
        case Compositor::ShellWindowActionStatus::Admitted:
            ++m_pending->windowIndex;
            if (m_pending->windowIndex < m_pending->windowIds.size()) {
                requestPendingWindow();
                return;
            }
            m_pending.reset();
            Q_EMIT operationFinished(result(token, TaskListOperationStatus::Committed,
                                            {}, {}));
            return;
        case Compositor::ShellWindowActionStatus::Stale:
            m_pending.reset();
            Q_EMIT operationFinished(result(token, TaskListOperationStatus::StaleGeneration,
                                            server.failureCode, server.message));
            return;
        case Compositor::ShellWindowActionStatus::Unauthorized:
        case Compositor::ShellWindowActionStatus::ControlDisabled:
            m_pending.reset();
            Q_EMIT operationFinished(result(token, TaskListOperationStatus::Unavailable,
                                            server.failureCode, server.message));
            return;
        case Compositor::ShellWindowActionStatus::UnknownWindow:
            m_pending.reset();
            Q_EMIT operationFinished(result(token, TaskListOperationStatus::Rejected,
                                            server.failureCode, server.message));
            return;
        }
    }

    void requestPendingWindow()
    {
        if (!m_pending || m_pending->kind != PendingKind::Window
            || m_pending->windowIndex >= m_pending->windowIds.size()) {
            return;
        }
        QString error;
        const quint64 token = m_pending->outerToken;
        const bool sent = m_windowActions.request(
            m_pending->windowAction,
            m_pending->windowIds.at(m_pending->windowIndex),
            m_pending->windowGeneration, &error);
        if (sent || !m_pending) {
            return;
        }
        m_pending.reset();
        Q_EMIT operationFinished(result(
            token, TaskListOperationStatus::Unavailable,
            QStringLiteral("window-actions-unavailable"),
            error.isEmpty()
                ? QStringLiteral("authenticated window actions are unavailable")
                : error));
    }

    void finishContainerAction(const TaskListOperationResult &innerResult)
    {
        if (!m_pending || m_pending->kind != PendingKind::Container) {
            return;
        }
        if (m_forwardingContainer && m_pending->innerToken == 0) {
            m_deferredContainerResults.append(innerResult);
            return;
        }
        if (innerResult.token != m_pending->innerToken) {
            return;
        }
        TaskListOperationResult outerResult = innerResult;
        outerResult.token = m_pending->outerToken;
        m_pending.reset();
        Q_EMIT operationFinished(outerResult);
    }

    void drainContainerResults()
    {
        const QList<TaskListOperationResult> deferred =
            std::exchange(m_deferredContainerResults, {});
        for (const auto &deferredResult : deferred) {
            finishContainerAction(deferredResult);
        }
    }

    quint64 allocateToken()
    {
        if (m_nextToken == 0) {
            return 0;
        }
        const quint64 token = m_nextToken;
        m_nextToken = token == std::numeric_limits<quint64>::max()
            ? 0 : token + 1;
        return token;
    }

    void emitImmediate(const TaskListOperationResult &operationResult)
    {
        Q_EMIT operationFinished(operationResult);
    }

    ShellTaskList::TaskListSource &m_source;
    ShellTaskList::Producer::TaskListOperationAuthority &m_authority;
    ShellTaskListApplet::TaskListAppletOperationPort &m_containerOperations;
    ShellWindowActionsClient::ShellWindowActionsClient &m_windowActions;
    std::optional<Pending> m_pending;
    QList<TaskListOperationResult> m_deferredContainerResults;
    quint64 m_nextToken = 1;
    bool m_forwardingContainer = false;
    QString m_seenAuthorityOwner;
};

TaskListAppletComposition::TaskListAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    const QDBusConnection &sessionBus,
    ShellWindowActionsClient::ShellWindowActionsClient &windowActions,
    QStringList applicationRoots, QStringList iconRoots,
    QStringList iconThemes)
    : m_ownedSource(std::make_unique<ShellTaskList::TaskListSource>())
    , m_ownedProducerTransport(std::make_unique<
          ShellTaskList::Producer::QtTaskListProducerTransport>(sessionBus))
    , m_ownedProducer(std::make_unique<
          ShellTaskList::Producer::TaskListFactsProducer>(
              *m_ownedProducerTransport, *m_ownedSource))
    , m_ownedOperationTransport(std::make_unique<
          ShellTaskList::Operations::QtTaskListOperationTransport>(sessionBus))
    , m_ownedOperationAdapter(std::make_unique<
          ShellTaskList::Operations::TaskListOperationAdapter>(
              *m_ownedProducer, *m_ownedOperationTransport))
    , m_ownedContainerBridge(std::make_unique<
          ShellTaskListApplet::TaskListAppletOperationBridge>(
              *m_ownedOperationAdapter))
    , m_iconResolver(std::make_unique<Icons::DesktopEntryIconResolver>(
          std::move(applicationRoots)))
    , m_iconThemeLocator(std::make_unique<Icons::IconThemeLocator>(
          std::move(iconRoots), std::move(iconThemes)))
{
    compose(catalog, policy, *m_ownedSource, *m_ownedProducer,
            *m_ownedContainerBridge, windowActions);
}

TaskListAppletComposition::TaskListAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    ShellTaskList::TaskListSource &source,
    ShellTaskList::Producer::TaskListOperationAuthority &authority,
    ShellTaskListApplet::TaskListAppletOperationPort &containerOperations,
    ShellWindowActionsClient::ShellWindowActionsClient &windowActions)
{
    compose(catalog, policy, source, authority, containerOperations,
            windowActions);
}

TaskListAppletComposition::~TaskListAppletComposition()
{
    stop();
}

void TaskListAppletComposition::compose(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    ShellTaskList::TaskListSource &source,
    ShellTaskList::Producer::TaskListOperationAuthority &authority,
    ShellTaskListApplet::TaskListAppletOperationPort &containerOperations,
    ShellWindowActionsClient::ShellWindowActionsClient &windowActions)
{
    m_router = std::make_unique<TaskListWindowOperationRouter>(
        source, authority, containerOperations, windowActions);
    m_access = std::make_unique<ShellTaskListApplet::TaskListAppletController>(
        source, authority, *m_router, taskListGrants(catalog, policy),
        [this](const QString &applicationId) {
            return m_iconResolver
                ? m_iconResolver->iconNameForAppId(applicationId) : QString{};
        },
        [this](const QString &iconName) {
            return m_iconThemeLocator
                && m_iconThemeLocator->hasIcon(iconName, 18, 1.0, false);
        },
        [this](const QString &applicationId, const QString &reportedName) {
            return m_iconResolver
                ? m_iconResolver->applicationDisplayName(applicationId, reportedName)
                : reportedName;
        });
}

bool TaskListAppletComposition::start(QString *error)
{
    if (!m_ownedProducer) {
        if (error) {
            error->clear();
        }
        return true;
    }
    return m_ownedProducer->start(error);
}

void TaskListAppletComposition::stop()
{
    if (m_ownedProducer) {
        m_ownedProducer->stop();
    }
}

ShellTaskListApplet::TaskListAppletController *
TaskListAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell

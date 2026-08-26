// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwintaskidentitymanager.h"

#include "managedwindowregistry.h"

#include <window.h>
#include <workspace.h>

#include <QScopeGuard>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

KWinTaskIdentityManager::KWinTaskIdentityManager(
    ManagedWindowRegistry &registry,
    TaskTopologyProvider topology,
    TaskPageActivator activatePage,
    TaskContainerMinimizer minimizeContainer,
    TaskEventSuppression eventsSuppressed,
    QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_topology(std::move(topology))
    , m_activatePage(std::move(activatePage))
    , m_minimizeContainer(std::move(minimizeContainer))
    , m_eventsSuppressed(std::move(eventsSuppressed))
{
    connect(KWin::workspace(), &KWin::Workspace::windowActivated,
            this, &KWinTaskIdentityManager::handleActivated);
}

KWinTaskIdentityManager::~KWinTaskIdentityManager()
{
    shutdown();
}

std::optional<QVector<TaskContainerIdentity>> KWinTaskIdentityManager::buildPlans(
    const Hybrid::WindowTopology &topology,
    const QString &preferredActiveWindowId,
    QString *error) const
{
    const auto validation = topology.validate();
    if (!validation.valid) {
        fail(error, validation.message);
        return std::nullopt;
    }
    QVector<TaskContainerIdentity> next;
    next.reserve(topology.containerIds().size());
    for (const auto &containerId : topology.containerIds()) {
        const auto *container = topology.container(containerId);
        if (!container) {
            fail(error, QStringLiteral("task adapter lost container '%1'").arg(containerId));
            return std::nullopt;
        }

        QString preferred = preferredActiveWindowId;
        if (!container->findWindow(preferred)) {
            preferred = primaryWindowId(containerId);
        }
        auto plan = HybridTaskIdentityPolicy::planContainer(*container, preferred, error);
        if (!plan) {
            return std::nullopt;
        }
        for (const auto &member : plan->members) {
            if (!m_registry.window(member.windowId)
                || m_registry.owner(member.windowId) != containerId) {
                fail(error, QStringLiteral(
                                "task adapter cannot resolve grouped member '%1'")
                                .arg(member.windowId));
                return std::nullopt;
            }
        }
        next.append(std::move(*plan));
    }
    return next;
}

bool KWinTaskIdentityManager::synchronize(
    const Hybrid::WindowTopology &topology,
    QString *error)
{
    if (error) {
        error->clear();
    }
    if (m_shutdown) {
        return fail(error, QStringLiteral("task identity manager is shut down"));
    }
    const QString activeId = m_registry.windowId(KWin::workspace()->activeWindow());
    auto next = buildPlans(topology, activeId, error);
    if (!next) {
        return false;
    }

    ++m_applyDepth;
    const auto applyGuard = qScopeGuard([this] { --m_applyDepth; });
    // Expose the next primary before suppressing the prior one. KWin consumes
    // this compositor-thread burst after the signal returns, so users never see
    // a zero-entry container while its active member changes.
    for (const auto &plan : *next) {
        if (auto *primary = m_registry.window(plan.primaryWindowId)) {
            primary->setSkipTaskbar(false);
            primary->setSkipSwitcher(false);
        }
    }
    for (const auto &plan : *next) {
        for (const auto &member : plan.members) {
            if (member.primary) {
                continue;
            }
            auto *window = m_registry.window(member.windowId);
            window->setSkipTaskbar(true);
            window->setSkipSwitcher(true);
        }
    }

    // AGENT-GUARD: Publish policy values only after every live pointer was
    // preflighted. A rejected synchronize must retain the prior signal map and
    // must not partially suppress an unrelated task identity.
    m_plans = std::move(*next);
    reconnect();
    return true;
}

QString KWinTaskIdentityManager::primaryWindowId(const QString &containerId) const
{
    for (const auto &plan : m_plans) {
        if (plan.containerId == containerId) {
            return plan.primaryWindowId;
        }
    }
    return {};
}

bool KWinTaskIdentityManager::eventsAreSuppressed() const
{
    return m_shutdown || m_applyDepth > 0
        || (m_eventsSuppressed && m_eventsSuppressed());
}

void KWinTaskIdentityManager::reconnect()
{
    for (const auto &connections : std::as_const(m_windowConnections)) {
        for (const auto &connection : connections) {
            disconnect(connection);
        }
    }
    m_windowConnections.clear();

    for (const auto &plan : m_plans) {
        for (const auto &member : plan.members) {
            auto *window = m_registry.window(member.windowId);
            if (!window) {
                continue;
            }
            auto &connections = m_windowConnections[member.windowId];
            connections.append(connect(
                window, &KWin::Window::minimizedChanged, this,
                [this, id = member.windowId] { handleMinimizedChanged(id); }));
            connections.append(connect(
                window, &KWin::Window::skipTaskbarChanged, this,
                [this, id = member.windowId] { enforceMember(id); }));
            connections.append(connect(
                window, &KWin::Window::skipSwitcherChanged, this,
                [this, id = member.windowId] { enforceMember(id); }));
        }
    }
}

void KWinTaskIdentityManager::handleActivated(KWin::Window *window)
{
    if (eventsAreSuppressed()) {
        return;
    }
    const auto windowId = m_registry.windowId(window);
    const auto *member = HybridTaskIdentityPolicy::findMember(m_plans, windowId);
    if (!member) {
        // Dialogs and transients intentionally stay outside topology and retain
        // their native task/switcher policy.
        return;
    }
    const auto decision = HybridTaskIdentityPolicy::decide(
        member, TaskIdentityEvent::Activated);
    if (decision.hasAction()) {
        applyDecision(decision);
        return;
    }

    QString error;
    if (!m_topology || !synchronize(m_topology(), &error)) {
        warnFailure(QLatin1StringView("primary activation"), windowId, error);
    }
}

void KWinTaskIdentityManager::handleMinimizedChanged(const QString &windowId)
{
    if (eventsAreSuppressed()) {
        return;
    }
    auto *window = m_registry.window(windowId);
    const auto *member = HybridTaskIdentityPolicy::findMember(m_plans, windowId);
    if (!window || !member) {
        return;
    }
    applyDecision(HybridTaskIdentityPolicy::decide(
        member, window->isMinimized() ? TaskIdentityEvent::Minimized
                                      : TaskIdentityEvent::Unminimized));
}

void KWinTaskIdentityManager::enforceMember(const QString &windowId)
{
    if (eventsAreSuppressed()) {
        return;
    }
    auto *window = m_registry.window(windowId);
    const auto *member = HybridTaskIdentityPolicy::findMember(m_plans, windowId);
    if (!window || !member
        || (window->skipTaskbar() == member->skipTaskbar
            && window->skipSwitcher() == member->skipSwitcher)) {
        return;
    }
    ++m_applyDepth;
    const auto applyGuard = qScopeGuard([this] { --m_applyDepth; });
    window->setSkipTaskbar(member->skipTaskbar);
    window->setSkipSwitcher(member->skipSwitcher);
}

void KWinTaskIdentityManager::applyDecision(const TaskIdentityDecision &decision)
{
    if (!decision.hasAction() || eventsAreSuppressed()) {
        return;
    }
    QString error;
    bool success = false;
    {
        ++m_applyDepth;
        const auto applyGuard = qScopeGuard([this] { --m_applyDepth; });
        auto *window = m_registry.window(decision.windowId);
        if (!window) {
            error = QStringLiteral("task event window is no longer managed");
        } else if (decision.action == TaskIdentityAction::ActivatePage) {
            // AGENT-CONTRACT: Re-hide before the synchronous topology command.
            // The candidate page is revealed only by its committed scene plan;
            // rejection leaves this secondary minimized and excluded.
            if (decision.hideBeforeAction && !window->isMinimized()) {
                window->setMinimized(true);
            }
            success = m_activatePage
                && m_activatePage(decision.containerId, decision.pageId, &error);
        } else if (decision.action == TaskIdentityAction::MinimizeContainer) {
            success = m_minimizeContainer
                && m_minimizeContainer(decision.containerId, &error);
            if (!success && window->isMinimized()) {
                // Do not leave a single tiled leaf missing when the group-level
                // presentation action rejects.
                window->setMinimized(false);
            }
        }
    }

    if (!success) {
        if (error.isEmpty()) {
            error = QStringLiteral("required task identity callback is unavailable");
        }
        warnFailure(QLatin1StringView("event routing"), decision.windowId, error);
        return;
    }
    if (decision.action == TaskIdentityAction::ActivatePage && m_topology) {
        if (!synchronize(m_topology(), &error)) {
            warnFailure(QLatin1StringView("page activation refresh"),
                        decision.windowId, error);
        }
    }
}

void KWinTaskIdentityManager::warnFailure(
    QLatin1StringView operation,
    const QString &windowId,
    const QString &error) const
{
    if (!error.isEmpty()) {
        qWarning("QindaQt task identity %s failed for '%s': %s",
                 qPrintable(QString(operation)), qPrintable(windowId), qPrintable(error));
    }
}

void KWinTaskIdentityManager::shutdown() noexcept
{
    if (m_shutdown) {
        return;
    }
    for (const auto &connections : std::as_const(m_windowConnections)) {
        for (const auto &connection : connections) {
            disconnect(connection);
        }
    }
    m_windowConnections.clear();
    disconnect(KWin::workspace(), nullptr, this, nullptr);
    m_plans.clear();
    m_shutdown = true;
}

} // namespace QindaQt::Compositor::KWinIntegration

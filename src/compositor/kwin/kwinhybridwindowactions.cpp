// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "containercloseprompt.h"
#include "hybridchromesyncscheduler.h"
#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime.h"
#include "kwinchromemanager.h"
#include "kwinhybridscene.h"
#include "kwininteractionfilter.h"
#include "kwinmemberpolicy.h"
#include "managedwindowregistry.h"

#include <window.h>
#include <workspace.h>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void warnActionFailure(QLatin1StringView operation,
                       const HybridRuntimeResult &result)
{
    if (result.status == HybridRuntimeStatus::Rejected
        || result.status == HybridRuntimeStatus::Unsupported
        || result.status == HybridRuntimeStatus::NeedsGeometry) {
        qWarning("QindaQt Hybrid %s failed: %s",
                 qPrintable(QString(operation)), qPrintable(result.message));
    }
}

void collectPageWindowIds(const Core::LayoutNode &node, QStringList *windowIds)
{
    if (node.isLeaf()) {
        windowIds->append(node.windowId());
        return;
    }
    if (node.firstChild()) {
        collectPageWindowIds(*node.firstChild(), windowIds);
    }
    if (node.secondChild()) {
        collectPageWindowIds(*node.secondChild(), windowIds);
    }
}

} // namespace

bool KWinHybridSession::restoreMemberFocusForInteraction(QString *error)
{
    // AGENT-GUARD: Member focus mode owns native hidden/fullscreen state that
    // is intentionally outside WindowRestoreState. Restore it before any
    // user operation can change active pages, membership, or group geometry;
    // doing so after a scene commit can overwrite the new presentation.
    return !m_memberPolicy || m_memberPolicy->restoreForTopologyMutation(error);
}

bool KWinHybridSession::restoreMemberFocusForLifecycleChange(QString *error)
{
    return !m_memberPolicy || m_memberPolicy->restoreForLifecycleMutation(error);
}

std::optional<KWinHybridSession::ActiveKeyboardContext>
KWinHybridSession::activeKeyboardContext(QLatin1StringView operation) const
{
    if (!ready() || !m_inputFilter || !m_inputFilter->installed()) {
        qWarning("QindaQt keyboard %s is unavailable: input is not ready",
                 qPrintable(QString(operation)));
        return std::nullopt;
    }
    auto *const active = KWin::workspace()->activeWindow();
    const QString windowId = m_registry.windowId(active);
    if (windowId.isEmpty()) {
        qWarning("QindaQt keyboard %s is unavailable: no active managed window",
                 qPrintable(QString(operation)));
        return std::nullopt;
    }

    const auto &topology = m_runtime->topology();
    if (topology.isIndependent(windowId)) {
        return ActiveKeyboardContext{windowId, {}};
    }
    const auto owner = topology.ownerOf(windowId);
    if (!owner) {
        qWarning("QindaQt keyboard %s is unavailable: active window is not in topology",
                 qPrintable(QString(operation)));
        return std::nullopt;
    }
    return ActiveKeyboardContext{windowId, *owner};
}

void KWinHybridSession::startKeyboardDock()
{
    const auto context = activeKeyboardContext(QLatin1StringView("docking"));
    if (!context) {
        return;
    }
    QString error;
    if (!restoreMemberFocusForInteraction(&error)) {
        qWarning("QindaQt keyboard docking could not leave member focus: %s",
                 qPrintable(error));
        return;
    }
    const HybridInput::HitTarget source{
        HybridInput::HitKind::MemberTitle,
        context->containerId,
        context->windowId,
        {}};
    if (!m_inputFilter->beginKeyboardDock(source)) {
        qWarning("QindaQt keyboard docking could not acquire the active window");
    }
}

void KWinHybridSession::startKeyboardMove()
{
    const auto context = activeKeyboardContext(QLatin1StringView("group move"));
    if (!context) {
        return;
    }
    if (context->containerId.isEmpty()) {
        qWarning("QindaQt keyboard group move requires an active grouped window");
        return;
    }
    QString error;
    if (!restoreMemberFocusForInteraction(&error)) {
        qWarning("QindaQt keyboard group move could not leave member focus: %s",
                 qPrintable(error));
        return;
    }
    if (m_placement->isMaximized(context->containerId)
        || !m_sceneFactory->committedLayout(context->containerId)) {
        qWarning("QindaQt keyboard group move has no available placement");
        return;
    }
    const HybridInput::HitTarget source{
        HybridInput::HitKind::OuterTitle, context->containerId, {}, {}};
    if (!m_inputFilter->beginKeyboardMove(source)) {
        qWarning("QindaQt keyboard group move could not acquire input");
    }
}

void KWinHybridSession::startKeyboardDividerResize()
{
    const auto context = activeKeyboardContext(QLatin1StringView("split resize"));
    if (!context) {
        return;
    }
    if (context->containerId.isEmpty()) {
        qWarning("QindaQt keyboard split resize requires an active grouped window");
        return;
    }
    QString error;
    if (!restoreMemberFocusForInteraction(&error)) {
        qWarning("QindaQt keyboard split resize could not leave member focus: %s",
                 qPrintable(error));
        return;
    }
    const QString splitId = m_runtime->activePageFirstSplitId(context->containerId);
    const auto layout = m_sceneFactory->committedLayout(context->containerId);
    if (splitId.isEmpty() || !layout
        || !layout->activePage.splits.contains(splitId)) {
        qWarning("QindaQt keyboard split resize has no active-page divider");
        return;
    }
    const HybridInput::HitTarget source{
        HybridInput::HitKind::Divider,
        context->containerId,
        {},
        splitId};
    if (!m_inputFilter->beginKeyboardDividerResize(source)) {
        qWarning("QindaQt keyboard split resize could not acquire input");
    }
}

void KWinHybridSession::startKeyboardContainerResize()
{
    const auto context = activeKeyboardContext(QLatin1StringView("group resize"));
    if (!context) {
        return;
    }
    if (context->containerId.isEmpty()) {
        qWarning("QindaQt keyboard group resize requires an active grouped window");
        return;
    }
    QString error;
    if (!restoreMemberFocusForInteraction(&error)) {
        qWarning("QindaQt keyboard group resize could not leave member focus: %s",
                 qPrintable(error));
        return;
    }
    if (m_placement->isMaximized(context->containerId)
        || !m_sceneFactory->committedLayout(context->containerId)) {
        qWarning("QindaQt keyboard group resize has no available placement");
        return;
    }
    const HybridInput::HitTarget source{
        HybridInput::HitKind::OuterResize,
        context->containerId,
        {},
        {},
        Qt::RightEdge | Qt::BottomEdge};
    if (!m_inputFilter->beginKeyboardContainerResize(source)) {
        qWarning("QindaQt keyboard group resize could not acquire input");
    }
}

void KWinHybridSession::handleWindowAction(
    const QString &containerId, HybridChrome::WindowAction action)
{
    QString error;
    if (!dispatchGroupWindowAction(containerId, action, &error)) {
        qWarning("QindaQt Hybrid group window action failed: %s",
                 qPrintable(error));
    }
}

bool KWinHybridSession::dispatchGroupWindowAction(
    const QString &containerId,
    HybridChrome::WindowAction action,
    QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        if (error) {
            *error = QStringLiteral("group window action references a stale group");
        }
        return false;
    }
    if (!restoreMemberFocusForInteraction(error)) {
        return false;
    }
    switch (action) {
    case HybridChrome::WindowAction::Close:
        return requestCloseContainer(containerId, error);
    case HybridChrome::WindowAction::Minimize:
        minimizeContainer(containerId);
        return true;
    case HybridChrome::WindowAction::Maximize:
        return m_placement->maximize(containerId, error);
    case HybridChrome::WindowAction::Restore:
        return m_placement->restore(containerId, error);
    }
    if (error) {
        *error = QStringLiteral("unknown group window action");
    }
    return false;
}

void KWinHybridSession::handleWindowsChanged()
{
    if (!ready() || m_applyingWindowAction || m_synchronizingChrome
        || (m_sceneFactory && m_sceneFactory->applyingWindowStates())) {
        return;
    }
    // The shared scheduler permits only one queued pass across caption/frame/
    // minimize bursts. Scene-owned state application and window actions each
    // perform their own explicit final synchronization and are suppressed
    // above, avoiding O(member-count) redundant full publications per frame.
    if (m_chromeSyncScheduler) {
        m_chromeSyncScheduler->windowsChanged();
    }
}

void KWinHybridSession::reconcileMinimizedContainers()
{
    if (!ready() || m_applyingWindowAction) {
        return;
    }
    m_applyingWindowAction = true;
    const auto ids = m_minimizedContainers.values();
    for (const auto &containerId : ids) {
        bool allMinimized = true;
        for (const auto &windowId : m_runtime->topology().windowIds(containerId)) {
            const auto *window = m_registry.window(windowId);
            allMinimized = allMinimized && window && window->isMinimized();
        }
        if (allMinimized) {
            continue;
        }
        m_minimizedContainers.remove(containerId);
        QStringList activePageWindowIds;
        const auto *container = m_runtime->topology().container(containerId);
        const auto *activePage = container
            ? container->page(container->activePageId()) : nullptr;
        if (activePage) {
            collectPageWindowIds(activePage->root(), &activePageWindowIds);
        }
        // AGENT-GUARD: Restoring the container's single task identity exposes
        // only its active page. Inactive-page leaves remain minimized until a
        // page activation reflow makes them the visible page.
        for (const auto &windowId : activePageWindowIds) {
            if (auto *window = m_registry.window(windowId)) {
                window->setMinimized(false);
            }
        }
    }
    m_applyingWindowAction = false;
    synchronizeChrome();
}

void KWinHybridSession::minimizeContainer(const QString &containerId)
{
    m_minimizedContainers.insert(containerId);
    m_applyingWindowAction = true;
    for (const auto &windowId : m_runtime->topology().windowIds(containerId)) {
        if (auto *window = m_registry.window(windowId)) {
            window->setMinimized(true);
        }
    }
    m_applyingWindowAction = false;
    synchronizeChrome();
}

bool KWinHybridSession::requestCloseContainer(const QString &containerId,
                                              QString *error)
{
    const auto memberCount = m_runtime->topology().windowIds(containerId).size();
    // Scene chrome has no QWidget/QWindow by design. The prompt is itself a
    // KWin-internal window and must not depend on a fake chrome input surface.
    if (m_closePrompt->request(containerId, memberCount, nullptr)) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("group close prompt rejected the request");
    }
    return false;
}

void KWinHybridSession::handleCloseDecision(
    const QString &containerId, ContainerCloseDecision decision)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        return;
    }
    if (decision == ContainerCloseDecision::Cancel) {
        return;
    }
    if (decision == ContainerCloseDecision::Ungroup) {
        QString error;
        if (!restoreMemberFocusForInteraction(&error)) {
            qWarning("QindaQt container ungroup could not leave member focus: %s",
                     qPrintable(error));
            return;
        }
        const auto result = m_runtime->releaseContainer(containerId);
        warnActionFailure(QLatin1StringView("container ungroup"), result);
        if (result.topologyChanged()) {
            m_placement->forgetContainer(containerId);
            m_minimizedContainers.remove(containerId);
            synchronizeChrome();
        }
        return;
    }
    closeAllMembers(containerId);
}

void KWinHybridSession::closeAllMembers(const QString &containerId)
{
    // AGENT-GUARD: Copy IDs before requesting any close. Each asynchronous
    // removal normalizes topology and would otherwise invalidate iteration.
    const auto ids = m_runtime->topology().windowIds(containerId);
    for (const auto &windowId : ids) {
        if (auto *window = m_registry.window(windowId)) {
            window->closeWindow();
        }
    }
}

bool KWinHybridSession::detachNativeMember(const QString &containerId,
                                           const QString &windowId,
                                           QString *error)
{
    if (!ready() || m_registry.owner(windowId) != containerId) {
        if (error) {
            *error = QStringLiteral("native title drag source is not a grouped member");
        }
        return false;
    }
    HybridInput::InteractionIntent intent;
    intent.kind = HybridInput::InteractionKind::MemberDock;
    intent.phase = HybridInput::IntentPhase::Commit;
    intent.source = {HybridInput::HitKind::MemberTitle,
                     containerId, windowId, {}};
    // AGENT-CONTRACT: MemberDock + invalid target is the existing atomic
    // DetachMember path. Native KDecoration moves must not invent a second
    // topology mutation or weaken the exact Meta+Shift gesture contract.
    const auto result = m_runtime->handleIntent(intent);
    if (!result.topologyChanged()) {
        if (error) {
            *error = result.message.isEmpty()
                ? QStringLiteral("native title drag did not detach its member")
                : result.message;
        }
        return false;
    }
    if (!m_runtime->topology().container(containerId)) {
        m_placement->forgetContainer(containerId);
        m_minimizedContainers.remove(containerId);
    }
    synchronizeChrome();
    return true;
}

} // namespace QindaQt::Compositor::KWinIntegration

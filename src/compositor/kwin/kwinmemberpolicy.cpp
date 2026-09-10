// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinmemberpolicy.h"

#include "kwinhybridscene.h"
#include "kwinmemberpolicyplatform.h"
#include "managedwindowregistry.h"

#include <KDecoration3/Decoration>

#include <window.h>

#include <optional>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto ContainerMemberProperty = "qindaqtContainerMember";

void collectWindowIds(const Core::LayoutNode &node, QStringList *result)
{
    if (node.isLeaf()) {
        result->append(node.windowId());
        return;
    }
    if (node.firstChild()) {
        collectWindowIds(*node.firstChild(), result);
    }
    if (node.secondChild()) {
        collectWindowIds(*node.secondChild(), result);
    }
}

void setContainerMemberProperty(KWin::Window *window, bool member)
{
    auto *decoration = window ? window->decoration() : nullptr;
    if (!decoration) {
        return;
    }
    // AGENT-CONTRACT: QindaDecoration reads this process-local property to
    // drop its resize-only borders on grouped members: native member resize
    // is vetoed (ADR-0117), so the decoration must not advertise it. Written
    // on every membership change in reconnectGroupedWindows()/shutdown() so
    // it cannot go stale relative to the committed group set.
    decoration->setProperty(ContainerMemberProperty, member);
    decoration->update();
}

std::optional<NativeQuickTileEdge> pureQuickTileEdge(KWin::QuickTileMode mode)
{
    // AGENT-GUARD: Only a bare single-edge request maps to "movement within
    // the container". A corner combo, Maximize, or Custom quick-tile has no
    // single-direction equivalent among the existing dock zones and is left
    // alone here; Maximize already routes through member focus mode via
    // maximizedChanged.
    if (mode == KWin::QuickTileMode(KWin::QuickTileFlag::Left)) {
        return NativeQuickTileEdge::Left;
    }
    if (mode == KWin::QuickTileMode(KWin::QuickTileFlag::Right)) {
        return NativeQuickTileEdge::Right;
    }
    if (mode == KWin::QuickTileMode(KWin::QuickTileFlag::Top)) {
        return NativeQuickTileEdge::Top;
    }
    if (mode == KWin::QuickTileMode(KWin::QuickTileFlag::Bottom)) {
        return NativeQuickTileEdge::Bottom;
    }
    return std::nullopt;
}

} // namespace

KWinMemberPolicyManager::KWinMemberPolicyManager(
    ManagedWindowRegistry &registry,
    KWinChromeManager &chrome,
    NativeMemberDetach detach,
    MemberEventSuppression eventsSuppressed,
    NativeMemberQuickTile quickTileRequest,
    QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_chrome(chrome)
    , m_platform(std::make_unique<KWinMemberPolicyPlatform>(registry, chrome,
                                                            std::move(detach)))
    , m_policy(std::make_unique<HybridMemberPolicy>(*m_platform))
    , m_eventsSuppressed(std::move(eventsSuppressed))
    , m_quickTileRequest(std::move(quickTileRequest))
{
    connect(&m_registry, &ManagedWindowRegistry::managedWindowClosed,
            this, [this](const QString &windowId, const QString &) {
                handleClosed(windowId);
            });
}

bool KWinMemberPolicyManager::eventsAreSuppressed() const
{
    return m_shutdown || m_shutdownPresentationRestored
        || (m_eventsSuppressed && m_eventsSuppressed());
}

KWinMemberPolicyManager::~KWinMemberPolicyManager()
{
    if (!m_shutdown) {
        restorePresentationForShutdown();
    }
    shutdown();
}

bool KWinMemberPolicyManager::synchronize(const Hybrid::WindowTopology &topology,
                                          const KWinHybridSceneFactory &scene,
                                          QString *error)
{
    if (m_shutdown) {
        if (error) {
            *error = QStringLiteral("member policy manager is shut down");
        }
        return false;
    }
    QVector<MemberGroupBaseline> groups;
    for (const auto &containerId : topology.containerIds()) {
        const auto *container = topology.container(containerId);
        const auto layout = scene.committedLayout(containerId);
        if (!container || !layout) {
            if (error) {
                *error = QStringLiteral("member policy lacks committed layout for '%1'")
                             .arg(containerId);
            }
            return false;
        }
        MemberGroupBaseline group;
        group.containerId = containerId;
        group.outerFrame = layout->outerFrame;
        QStringList activeIds;
        if (const auto *page = container->page(container->activePageId())) {
            collectWindowIds(page->root(), &activeIds);
        }
        for (const auto &page : container->pages()) {
            QStringList pageIds;
            collectWindowIds(page.root(), &pageIds);
            for (const auto &windowId : pageIds) {
                auto *window = m_registry.window(windowId);
                if (!window) {
                    if (error) {
                        *error = QStringLiteral("member policy cannot resolve '%1'")
                                     .arg(windowId);
                    }
                    return false;
                }
                QRectF frame = m_registry.targetFrame(windowId);
                if (const auto placement = layout->activePage.members.constFind(windowId);
                    placement != layout->activePage.members.cend()) {
                    frame = placement->windowFrame;
                }
                group.members.append({
                    .windowId = windowId,
                    .frame = frame,
                    .minimized = window->isMinimized(),
                    .hidden = window->isHidden(),
                    .active = window->isActive(),
                    .activePage = activeIds.contains(windowId),
                });
            }
        }
        groups.append(std::move(group));
    }
    if (!m_policy->synchronize(groups, error)) {
        return false;
    }
    reconnectGroupedWindows(groups);
    enforceChromeVisibility();
    return true;
}

std::optional<MemberFocusState> KWinMemberPolicyManager::focusState(
    const QString &containerId) const
{
    return m_policy->focusState(containerId);
}

QVector<MemberFocusState> KWinMemberPolicyManager::focusStates() const
{
    return m_policy->focusStates();
}

bool KWinMemberPolicyManager::chromeVisible(const QString &containerId) const
{
    return !m_policy->focusState(containerId);
}

void KWinMemberPolicyManager::enforceChromeVisibility() const
{
    for (const auto &state : m_policy->focusStates()) {
        m_platform->setChromeVisible(state.containerId, false);
    }
}

bool KWinMemberPolicyManager::restoreForTopologyMutation(QString *error)
{
    if (m_shutdown) {
        if (error) {
            *error = QStringLiteral("member policy manager is shut down");
        }
        return false;
    }
    return m_policy->restoreForTopologyMutation(error);
}

bool KWinMemberPolicyManager::restoreForContainerAction(const QString &containerId,
                                                        QString *error)
{
    if (m_shutdown) {
        if (error) {
            *error = QStringLiteral("member policy manager is shut down");
        }
        return false;
    }
    return m_policy->restoreForContainerAction(containerId, error);
}

bool KWinMemberPolicyManager::restoreForLifecycleMutation(QString *error)
{
    if (m_shutdown) {
        if (error) {
            *error = QStringLiteral("member policy manager is shut down");
        }
        return false;
    }
    return m_policy->restoreForLifecycleMutation(error);
}

void KWinMemberPolicyManager::restorePresentationForShutdown() noexcept
{
    if (m_shutdown || m_shutdownPresentationRestored) {
        return;
    }
    QSet<QString> missing;
    for (const auto &state : m_policy->focusStates()) {
        const auto baseline = m_policy->focusBaseline(state.containerId);
        if (!baseline) {
            continue;
        }
        for (const auto &member : baseline->members) {
            if (!m_registry.window(member.windowId)) {
                missing.insert(member.windowId);
            }
        }
    }
    QString error;
    if (!m_policy->restoreForShutdown(std::move(missing), &error)) {
        qWarning("QindaQt member focus shutdown restore failed: %s",
                 qPrintable(error));
        return;
    }
    m_shutdownPresentationRestored = true;
}

void KWinMemberPolicyManager::shutdown() noexcept
{
    if (m_shutdown) {
        return;
    }
    // AGENT-GUARD: This phase is disconnect-only. The session has already
    // restored independent WindowRestoreState; replaying m_focusBaseline here
    // would replace exact independent frames with obsolete grouped frames.
    for (auto entry = m_windowConnections.cbegin();
         entry != m_windowConnections.cend(); ++entry) {
        // Former members regain their native resize borders with their
        // independent frames; a stale marker would keep suppressing them.
        setContainerMemberProperty(m_registry.window(entry.key()), false);
        for (const auto &connection : entry.value()) {
            disconnect(connection);
        }
    }
    m_windowConnections.clear();
    disconnect(&m_registry, nullptr, this, nullptr);
    m_shutdown = true;
}

void KWinMemberPolicyManager::reconnectGroupedWindows(
    const QVector<MemberGroupBaseline> &groups)
{
    QSet<QString> currentMembers;
    for (const auto &group : groups) {
        for (const auto &member : group.members) {
            currentMembers.insert(member.windowId);
        }
    }
    for (auto entry = m_windowConnections.cbegin();
         entry != m_windowConnections.cend(); ++entry) {
        if (!currentMembers.contains(entry.key())) {
            setContainerMemberProperty(m_registry.window(entry.key()), false);
        }
        for (const auto &connection : entry.value()) {
            disconnect(connection);
        }
    }
    m_windowConnections.clear();

    for (const auto &group : groups) {
        for (const auto &member : group.members) {
            auto *window = m_registry.window(member.windowId);
            if (!window) {
                continue;
            }
            setContainerMemberProperty(window, true);
            auto &connections = m_windowConnections[member.windowId];
            connections.append(connect(
                window, &KWin::Window::requestedTileChanged, this,
                [this, id = member.windowId, containerId = group.containerId, window] {
                    if (eventsAreSuppressed()) {
                        return;
                    }
                    const auto mode = window->requestedQuickTileMode();
                    if (mode == KWin::QuickTileMode(KWin::QuickTileFlag::None)) {
                        // AGENT-GUARD: recursion stop. The clear below
                        // re-enters this same handler through a nested
                        // requestTile(nullptr); it must see None here and
                        // return, or every clear would recurse forever.
                        return;
                    }
                    // AGENT-GUARD: requestedTileChanged fires synchronously
                    // inside Window::requestTile() for both X11 and Wayland
                    // windows (unlike quickTileModeChanged, which is
                    // asynchronous for xdg-shell clients), and Q_EMIT is the
                    // last statement in that function, so this nested clear
                    // completes before the outer request returns with no
                    // double-apply race. A grouped member's frame is owned by
                    // its container: every non-None request is cleared here,
                    // corner combos and Custom included, not only the pure
                    // single-edge modes that additionally redirect below.
                    window->setQuickTileMode(KWin::QuickTileFlag::None,
                                             window->frameGeometry().center());
                    const auto edge = pureQuickTileEdge(mode);
                    if (!edge) {
                        // Corner combos and Custom have no typed container-
                        // topology equivalent yet. The native mode is already
                        // cleared above, so the member stays where it is
                        // instead of tiling outside its container.
                        return;
                    }
                    QString error;
                    if (m_quickTileRequest
                        && !m_quickTileRequest(containerId, id, *edge, &error)) {
                        warnFailure(QLatin1StringView("quick-tile redirect"), id, error);
                    }
                }));
            connections.append(connect(
                window, &KWin::Window::interactiveMoveResizeStarted, this,
                [this, id = member.windowId, window] {
                    if (eventsAreSuppressed()) {
                        return;
                    }
                    if (!window->isInteractiveMove()
                        && m_policy->blocksInteractiveResize(id)) {
                        // AGENT-GUARD: A grouped member's frame changes only
                        // through container reflow (divider drags, outer
                        // resize, keyboard divider resize). Removing this
                        // veto lets a native member resize desynchronize the
                        // window from its tile until the next reflow. The
                        // direct cancel is reentrancy-safe on the pinned KWin
                        // (6.6): finishInteractiveMoveResize(cancel=true)
                        // replays the untouched initial geometry, and the
                        // emission tail in startInteractiveMoveResize() only
                        // reserves desktop-switching screen edges.
                        window->cancelInteractiveMoveResize();
                        return;
                    }
                    QString error;
                    (void)m_policy->interactiveMoveStarted(id, window->isInteractiveMove(),
                                                           &error);
                    warnFailure(QLatin1StringView("title drag"), id, error);
                }));
            connections.append(connect(
                window, &KWin::Window::maximizedChanged, this,
                [this, id = member.windowId, window] {
                    if (eventsAreSuppressed()) {
                        return;
                    }
                    QString error;
                    (void)m_policy->maximizedChanged(
                        id, window->maximizeMode() != KWin::MaximizeRestore, &error);
                    warnFailure(QLatin1StringView("maximize"), id, error);
                }));
            connections.append(connect(
                window, &KWin::Window::fullScreenChanged, this,
                [this, id = member.windowId, window] {
                    if (eventsAreSuppressed()) {
                        return;
                    }
                    QString error;
                    (void)m_policy->fullscreenChanged(id, window->isFullScreen(), &error);
                    warnFailure(QLatin1StringView("fullscreen"), id, error);
                }));
            connections.append(connect(
                window, &KWin::Window::minimizedChanged, this,
                [this, id = member.windowId, window] {
                    if (eventsAreSuppressed()) {
                        return;
                    }
                    QString error;
                    (void)m_policy->minimizedChanged(id, window->isMinimized(), &error);
                    warnFailure(QLatin1StringView("minimize"), id, error);
                }));
        }
    }
}

void KWinMemberPolicyManager::handleClosed(const QString &windowId)
{
    if (eventsAreSuppressed()) {
        return;
    }
    QString error;
    (void)m_policy->memberClosed(windowId, &error);
    warnFailure(QLatin1StringView("close"), windowId, error);
}

void KWinMemberPolicyManager::warnFailure(QLatin1StringView operation,
                                          const QString &windowId,
                                          const QString &error) const
{
    if (!error.isEmpty()) {
        qWarning("QindaQt member %s failed for '%s': %s",
                 qPrintable(QString(operation)), qPrintable(windowId), qPrintable(error));
    }
}

} // namespace QindaQt::Compositor::KWinIntegration

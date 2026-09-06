// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinmemberpolicy.h"

#include "kwinchromemanager.h"
#include "kwinhybridscene.h"
#include "managedwindowregistry.h"

#include <KDecoration3/Decoration>

#include <window.h>
#include <workspace.h>

#include <QPointer>

#include <optional>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto MemberFocusProperty = "qindaqtMemberFocusMode";

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

void setFocusProperty(KWin::Window *window, MemberFocusMode mode, bool enabled)
{
    auto *decoration = window ? window->decoration() : nullptr;
    if (!decoration) {
        return;
    }
    const auto value = !enabled ? QString{}
        : mode == MemberFocusMode::Maximized ? QStringLiteral("maximized")
                                             : QStringLiteral("fullscreen");
    // AGENT-CONTRACT: QindaDecoration reads this process-local property only
    // for its maximize/restore glyph. KWin's real maximize bit stays clear so
    // the temporary group presentation cannot acquire independent geometry.
    decoration->setProperty(MemberFocusProperty, value);
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

bool preflight(const ManagedWindowRegistry &registry,
               const MemberGroupBaseline &baseline,
               const QSet<QString> &missing,
               QString *error)
{
    for (const auto &member : baseline.members) {
        if (missing.contains(member.windowId)) {
            continue;
        }
        if (!registry.window(member.windowId)) {
            if (error) {
                *error = QStringLiteral("group member '%1' is no longer managed")
                             .arg(member.windowId);
            }
            return false;
        }
    }
    return true;
}

} // namespace

class KWinMemberPolicyManager::Platform final : public HybridMemberPolicyPlatform
{
public:
    Platform(ManagedWindowRegistry &registry,
             KWinChromeManager &chrome,
             NativeMemberDetach detach)
        : m_registry(registry)
        , m_chrome(chrome)
        , m_detach(std::move(detach))
    {
    }

    bool detachMember(const QString &containerId,
                      const QString &windowId,
                      const MemberGroupBaseline *focusBaseline,
                      QString *error) override
    {
        if (!m_detach) {
            if (error) {
                *error = QStringLiteral("native member detach callback is unavailable");
            }
            return false;
        }
        // The topology scene transaction restores the detached client and
        // reflows survivors atomically. Only clear presentation metadata after
        // it commits, so a rejected mutation leaves focus mode untouched.
        if (!m_detach(containerId, windowId, error)) {
            return false;
        }
        if (focusBaseline) {
            for (const auto &member : focusBaseline->members) {
                if (auto *window = m_registry.window(member.windowId)) {
                    setFocusProperty(window, MemberFocusMode::Maximized, false);
                    window->setHidden(member.hidden);
                }
            }
            setChromeVisible(containerId, true);
        }
        return true;
    }

    bool enterFocus(const MemberGroupBaseline &baseline,
                    const QString &windowId,
                    MemberFocusMode mode,
                    QString *error) override
    {
        if (!preflight(m_registry, baseline, {}, error)) {
            return false;
        }
        auto *focused = m_registry.window(windowId);
        if (!focused || !baseline.member(windowId)) {
            if (error) {
                *error = QStringLiteral("focus member is not in the committed group");
            }
            return false;
        }

        setChromeVisible(baseline.containerId, false);
        for (const auto &member : baseline.members) {
            auto *window = m_registry.window(member.windowId);
            setFocusProperty(window, mode, member.windowId == windowId);
            if (member.windowId != windowId) {
                window->setHidden(true);
                continue;
            }
            window->setHidden(false);
            window->setMinimized(false);
            if (mode == MemberFocusMode::Fullscreen) {
                if (!window->isFullScreen()) {
                    window->setFullScreen(true);
                }
            } else {
                if (window->isFullScreen()) {
                    window->setFullScreen(false);
                }
                if (window->maximizeMode() != KWin::MaximizeRestore) {
                    window->maximize(KWin::MaximizeRestore);
                }
                window->moveResize(baseline.outerFrame);
            }
        }
        KWin::workspace()->activateWindow(focused);
        return true;
    }

    bool restoreRejectedPresentation(const MemberGroupBaseline &baseline,
                                     const QString &windowId,
                                     MemberFocusMode mode,
                                     QString *error) override
    {
        const auto *member = baseline.member(windowId);
        auto *window = m_registry.window(windowId);
        if (!member || !window) {
            if (error) {
                *error = QStringLiteral(
                    "rejected focus member is absent from the committed group");
            }
            return false;
        }

        // AGENT-GUARD: This is a rejected peer request while another member
        // owns focus presentation. Do not use restoreGroup(), change hidden
        // state, or activate any window: each would reveal or unfocus the
        // accepted owner. HybridMemberPolicy keeps its applying guard active
        // while these KWin setters synchronously emit their state signals.
        if (mode == MemberFocusMode::Fullscreen && window->isFullScreen()) {
            window->setFullScreen(false);
        }
        if (window->maximizeMode() != KWin::MaximizeRestore) {
            window->maximize(KWin::MaximizeRestore);
        }
        if (window->requestedQuickTileMode()
                != KWin::QuickTileMode(KWin::QuickTileFlag::None)
            || window->quickTileMode()
                != KWin::QuickTileMode(KWin::QuickTileFlag::None)) {
            window->setQuickTileMode(KWin::QuickTileFlag::None,
                                     member->frame.center());
        }
        window->moveResize(member->frame);
        return true;
    }

    bool restoreGroup(const MemberGroupBaseline &baseline,
                      const QString &minimizeWindowId,
                      const QSet<QString> &missingWindowIds,
                      MemberRestoreActivation activationMode,
                      QString *error) override
    {
        if (!preflight(m_registry, baseline, missingWindowIds, error)) {
            return false;
        }
        QPointer<KWin::Window> preservedActivation;
        if (activationMode == MemberRestoreActivation::PreserveCurrent) {
            preservedActivation = KWin::workspace()->activeWindow();
        }
        KWin::Window *baselineActivation = nullptr;
        for (const auto &member : baseline.members) {
            if (missingWindowIds.contains(member.windowId)) {
                continue;
            }
            auto *window = m_registry.window(member.windowId);
            setFocusProperty(window, MemberFocusMode::Maximized, false);
            if (window->isFullScreen()) {
                window->setFullScreen(false);
            }
            if (window->maximizeMode() != KWin::MaximizeRestore) {
                window->maximize(KWin::MaximizeRestore);
            }
            window->setHidden(member.hidden);
            window->moveResize(member.frame);
            window->setMinimized(member.minimized
                                 || member.windowId == minimizeWindowId);
            if (member.active && member.windowId != minimizeWindowId
                && !member.minimized && !member.hidden) {
                baselineActivation = window;
            }
        }
        setChromeVisible(baseline.containerId, true);
        if (activationMode == MemberRestoreActivation::PreserveCurrent) {
            if (preservedActivation && !preservedActivation->isDeleted()
                && !preservedActivation->isMinimized()
                && !preservedActivation->isHidden()) {
                KWin::workspace()->activateWindow(preservedActivation);
            }
        } else if (baselineActivation) {
            KWin::workspace()->activateWindow(baselineActivation);
        }
        return true;
    }

    void setChromeVisible(const QString &containerId, bool visible) const
    {
        m_chrome.setOverlayVisible(containerId, visible);
    }

private:
    ManagedWindowRegistry &m_registry;
    KWinChromeManager &m_chrome;
    NativeMemberDetach m_detach;
};

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
    , m_platform(std::make_unique<Platform>(registry, chrome, std::move(detach)))
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

std::optional<MemberFocusState> KWinMemberPolicyManager::focusState() const
{
    return m_policy->focusState();
}

bool KWinMemberPolicyManager::chromeVisible(const QString &containerId) const
{
    const auto state = focusState();
    return !state || state->containerId != containerId;
}

void KWinMemberPolicyManager::enforceChromeVisibility() const
{
    const auto state = focusState();
    if (state) {
        m_platform->setChromeVisible(state->containerId, false);
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
    if (const auto baseline = m_policy->focusBaseline()) {
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
    for (const auto &connections : std::as_const(m_windowConnections)) {
        for (const auto &connection : connections) {
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
    for (const auto &connections : std::as_const(m_windowConnections)) {
        for (const auto &connection : connections) {
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

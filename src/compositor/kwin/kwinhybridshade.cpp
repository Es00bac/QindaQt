// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridcontainerplacement.h"
#include "hybridshadecontroller.h"
#include "kwinhybridgroupstacking.h"
#include "managedwindowregistry.h"

#include <compositor.h>
#include <scene/shadowitem.h>
#include <scene/windowitem.h>
#include <window.h>
#include <workspace.h>

#include <QPointer>
#include <QTimer>

#include <functional>
#include <optional>
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

// AGENT-NOTE: KWin 6.6 WorkspaceScene::preparePaintSimpleScreen() adds the
// opaque region of every child item -- visible or not -- for each window whose
// opacity is exactly 1.0, and collectDamage() then culls damage beneath it. A
// force-visible anchor whose client surface declares an opaque region (client-
// side decorations, Electron, Firefox, borderless clients) would stop the
// desktop beneath the rolled-up group from repainting, leaving a stale ghost
// of the member. Keeping the shaded anchor marginally below 1.0 removes it
// from occlusion; the shared chrome it carries renders indistinguishably.
constexpr qreal ShadedAnchorOpacity = 0.999;
constexpr int MaximumTransientDepth = 16;

struct ShadePlatformCallbacks final
{
    // KWin unhid a shaded member or one of its hidden transients, or mapped a
    // new transient of a shaded member. `window` is the affected window.
    std::function<void(const QString &memberId, KWin::Window *window)> revealed;
    // A shaded member closed; its platform state is already dropped.
    std::function<void(const QString &memberId)> closed;
    // The compositor scene was recreated, so every WindowItem is new.
    std::function<void()> sceneRecreated;
};

// AGENT-CONTRACT: real KWin adapter for HybridShadeMemberPlatform; the only
// place that touches KWin scene internals for shade. See ADR-0099 (and its
// 2026-09-12 follow-up) and docs/wiki/architecture/hybrid-chrome.md.
// Window::isHidden() removes a window from InputRedirection::findToplevel()'s
// pointer targets and from the focus chain; WindowItem::refVisible(
// PAINT_DISABLED_BY_HIDDEN) is the same ref-counted API KWin's minimize/genie
// effects use to keep an item paintable despite isHidden(). Every member's
// windowContainer (surface + decoration) and shadow are hidden explicitly, so
// KWin clearing isHidden() behind our back can never repaint content; it is
// reported through `revealed` so the controller re-applies the treatment.
class KWinShadeMemberPlatform final : public HybridShadeMemberPlatform
{
public:
    KWinShadeMemberPlatform(ManagedWindowRegistry &registry,
                            ShadePlatformCallbacks callbacks)
        : m_registry(registry)
        , m_callbacks(std::move(callbacks))
    {
    }

    ~KWinShadeMemberPlatform() override
    {
        // Shutdown restores members first; only drop subscriptions to `this`.
        for (auto &state : m_applied) {
            disconnectAll(state);
        }
        QObject::disconnect(m_windowAdded);
        QObject::disconnect(m_compositingToggled);
    }

    bool hideMember(const QString &windowId, QString *error) override
    {
        return hide(windowId, /*anchor=*/false, error);
    }
    bool hideAnchorContent(const QString &windowId, QString *error) override
    {
        return hide(windowId, /*anchor=*/true, error);
    }
    bool showMember(const QString &windowId, QString *) override { return show(windowId); }
    bool showAnchorContent(const QString &windowId, QString *) override { return show(windowId); }

private:
    // Exactly what this adapter changed for one member, so restore undoes
    // only that: a WindowItem recreated by a scene restart is never unref'd,
    // and items/transients hidden by someone else are never shown.
    struct AppliedShade final
    {
        QPointer<KWin::Window> window;
        QPointer<KWin::WindowItem> forcedItem;
        QPointer<KWin::Item> hiddenContainer;
        QPointer<KWin::Item> hiddenShadow;
        std::optional<qreal> originalOpacity;
        QList<QPointer<KWin::Window>> hiddenTransients;
        QList<QMetaObject::Connection> connections;
        bool anchor = false;
    };

    bool hide(const QString &windowId, bool anchor, QString *error)
    {
        auto *const window = m_registry.window(windowId);
        if (!window || window->isDeleted()) {
            return fail(error,
                        QStringLiteral("shade member '%1' is unavailable").arg(windowId));
        }
        auto *const item = window->windowItem();
        if (anchor && !item) {
            return fail(error,
                        QStringLiteral("shade anchor '%1' has no scene item").arg(windowId));
        }
        auto &state = m_applied[windowId];
        state.window = window;
        // AGENT-GUARD: content, decoration, and shadow are hidden before the
        // anchor is forced visible and before Window::setHidden() runs, so no
        // presented frame can show member content once shade has begun.
        hideContent(window, state);
        if (anchor) {
            if (state.forcedItem != item) {
                item->refVisible(KWin::WindowItem::PAINT_DISABLED_BY_HIDDEN);
                state.forcedItem = item;
            }
            lowerAnchorOpacity(window, state);
            state.anchor = true;
        }
        watch(windowId, window, state);
        hideTransients(windowId, window, state, 0);
        window->setHidden(true);
        updateGlobalWatch();
        return true;
    }

    bool show(const QString &windowId)
    {
        const auto found = m_applied.find(windowId);
        if (found == m_applied.end()) {
            // Never hidden here, or closed while shaded: nothing to restore.
            return true;
        }
        AppliedShade state = std::move(*found);
        m_applied.erase(found);
        // Disconnect first: our own setHidden(false) is not a reveal.
        disconnectAll(state);
        if (state.hiddenContainer) {
            state.hiddenContainer->setVisible(true);
        }
        if (state.hiddenShadow) {
            state.hiddenShadow->setVisible(true);
        }
        const auto window = state.window;
        if (window && !window->isDeleted()) {
            if (state.originalOpacity
                && qFuzzyCompare(window->opacity(), ShadedAnchorOpacity)) {
                window->setOpacity(*state.originalOpacity);
            }
            window->setHidden(false);
        }
        if (state.forcedItem) {
            state.forcedItem->unrefVisible(KWin::WindowItem::PAINT_DISABLED_BY_HIDDEN);
        }
        showTransients(state);
        updateGlobalWatch();
        return true;
    }

    void hideContent(KWin::Window *window, AppliedShade &state)
    {
        auto *const item = window->windowItem();
        if (!item) {
            return; // No scene item yet: Window::setHidden() keeps it unpainted.
        }
        auto *const container = item->windowContainer();
        if (container->explicitVisible()) {
            container->setVisible(false);
            state.hiddenContainer = container;
        }
        if (auto *const shadow = item->shadowItem(); shadow && shadow->explicitVisible()) {
            shadow->setVisible(false);
            state.hiddenShadow = shadow;
        }
    }

    void lowerAnchorOpacity(KWin::Window *window, AppliedShade &state)
    {
        if (window->opacity() > ShadedAnchorOpacity) {
            state.originalOpacity = window->opacity();
            window->setOpacity(ShadedAnchorOpacity);
        }
    }

    // AGENT-GUARD: dialogs and popups of a shaded member would otherwise float
    // over the vacated area as live, input-eligible ghosts of the hidden group.
    void hideTransients(const QString &memberId, KWin::Window *window,
                        AppliedShade &state, int depth)
    {
        const auto transients = window->transients();
        for (auto *const transient : transients) {
            if (!transient || transient->isDeleted() || depth >= MaximumTransientDepth) {
                continue;
            }
            const bool recorded = state.hiddenTransients.contains(transient);
            if (!recorded && transient->isHidden()) {
                continue; // Hidden by someone else: never ours to restore.
            }
            transient->setHidden(true);
            if (!recorded) {
                state.hiddenTransients.append(transient);
                state.connections.append(watchReveal(memberId, transient));
            }
            hideTransients(memberId, transient, state, depth + 1);
        }
    }

    static void showTransients(const AppliedShade &state)
    {
        for (const auto &transient : state.hiddenTransients) {
            if (transient && !transient->isDeleted()) {
                transient->setHidden(false);
            }
        }
    }

    // AGENT-CONTRACT: Workspace::activateWindow() calls setHidden(false)
    // itself (client xdg-activation, X11 _NET_ACTIVE_WINDOW, Alt-Tab,
    // unminimize, reflow); every such reveal must reach the controller.
    QMetaObject::Connection watchReveal(const QString &memberId, KWin::Window *window)
    {
        return QObject::connect(window, &KWin::Window::hiddenChanged, window,
                                [this, memberId, window] {
            if (!window->isHidden() && m_callbacks.revealed) {
                m_callbacks.revealed(memberId, window);
            }
        });
    }

    void watch(const QString &memberId, KWin::Window *window, AppliedShade &state)
    {
        if (!state.connections.isEmpty()) {
            return;
        }
        state.connections.append(watchReveal(memberId, window));
        state.connections.append(QObject::connect(
            window, &KWin::Window::shadowChanged, window, [this, memberId, window] {
                // WindowItem replaces its ShadowItem when the Shadow changes.
                if (const auto found = m_applied.find(memberId); found != m_applied.end()) {
                    hideContent(window, *found);
                }
            }));
        state.connections.append(QObject::connect(
            window, &KWin::Window::opacityChanged, window, [this, memberId, window] {
                const auto found = m_applied.find(memberId);
                if (found != m_applied.end() && found->anchor) {
                    lowerAnchorOpacity(window, *found);
                }
            }));
        state.connections.append(QObject::connect(
            window, &KWin::Window::closed, window, [this, memberId] {
                if (const auto found = m_applied.find(memberId); found != m_applied.end()) {
                    AppliedShade closedState = std::move(*found);
                    m_applied.erase(found);
                    disconnectAll(closedState);
                    showTransients(closedState);
                    updateGlobalWatch();
                }
                if (m_callbacks.closed) {
                    m_callbacks.closed(memberId);
                }
            }));
    }

    void updateGlobalWatch()
    {
        const bool needed = !m_applied.isEmpty();
        if (needed && !m_windowAdded) {
            auto *const workspace = KWin::workspace();
            m_windowAdded = QObject::connect(
                workspace, &KWin::Workspace::windowAdded, workspace,
                [this](KWin::Window *added) { handleWindowAdded(added); });
            if (auto *const compositor = KWin::Compositor::self()) {
                m_compositingToggled = QObject::connect(
                    compositor, &KWin::Compositor::compositingToggled, compositor,
                    [this](bool active) {
                        if (active && m_callbacks.sceneRecreated) {
                            m_callbacks.sceneRecreated();
                        }
                    });
            }
        } else if (!needed && m_windowAdded) {
            QObject::disconnect(m_windowAdded);
            QObject::disconnect(m_compositingToggled);
            m_windowAdded = {};
            m_compositingToggled = {};
        }
    }

    // A transient mapped for a shaded member is hidden via the reveal path.
    void handleWindowAdded(KWin::Window *added)
    {
        int depth = 0;
        for (auto *owner = added ? added->transientFor() : nullptr;
             owner && depth < MaximumTransientDepth;
             owner = owner->transientFor(), ++depth) {
            const auto ownerId = m_registry.windowId(owner);
            if (!ownerId.isEmpty() && m_applied.contains(ownerId) && m_callbacks.revealed) {
                m_callbacks.revealed(ownerId, added);
                return;
            }
        }
    }

    static void disconnectAll(AppliedShade &state)
    {
        for (const auto &connection : std::as_const(state.connections)) {
            QObject::disconnect(connection);
        }
        state.connections.clear();
    }

    ManagedWindowRegistry &m_registry;
    ShadePlatformCallbacks m_callbacks;
    QHash<QString, AppliedShade> m_applied;
    QMetaObject::Connection m_windowAdded;
    QMetaObject::Connection m_compositingToggled;
};

} // namespace

void KWinHybridSession::ensureShadeController()
{
    if (m_shadeController) {
        return;
    }
    ShadePlatformCallbacks callbacks;
    callbacks.revealed = [this](const QString &memberId, KWin::Window *window) {
        QString error;
        const auto result = m_shadeController
            ? m_shadeController->reassertMember(memberId, &error)
            : ShadeReassertResult::NotEnforced;
        if (result == ShadeReassertResult::Failed) {
            qWarning("QindaQt could not keep shaded member '%s' hidden: %s",
                     qPrintable(memberId), qPrintable(error));
        }
        if (result != ShadeReassertResult::Reapplied) {
            return;
        }
        // AGENT-GUARD: KWin reveals a window before it focuses it, so the
        // re-hidden window may still become active once activateWindow()
        // returns. Hand keyboard focus to the next shown window afterwards;
        // the focus chain never selects a hidden window.
        QTimer::singleShot(0, this, [target = QPointer<KWin::Window>(window)] {
            auto *const workspace = KWin::workspace();
            if (target && workspace && workspace->activeWindow() == target
                && target->isHidden()) {
                workspace->activateNextWindow(target);
            }
        });
    };
    callbacks.closed = [this](const QString &memberId) {
        const auto containerId = m_shadeController
            ? m_shadeController->containerOfMember(memberId) : QString{};
        QString error;
        if (containerId.isEmpty() || m_shadeController->memberClosed(memberId, &error)) {
            return;
        }
        qWarning("QindaQt shaded group '%s' lost its chrome anchor: %s",
                 qPrintable(containerId), qPrintable(error));
        // AGENT-GUARD: never leave surviving members hidden with no strip to
        // unroll. Unroll once the close lifecycle has settled the topology.
        QTimer::singleShot(0, this, [this, containerId] {
            QString unrollError;
            if (!m_placement->isShaded(containerId)
                || !unshadeContainer(containerId, &unrollError)) {
                forgetShadedContainer(containerId);
            }
        });
    };
    callbacks.sceneRecreated = [this] {
        const auto containerIds = m_shadeController
            ? m_shadeController->shadedContainerIds() : QStringList{};
        for (const auto &containerId : containerIds) {
            QString error;
            if (!m_shadeController->isReleasing(containerId)
                && !m_shadeController->reassertContainer(containerId, &error)) {
                qWarning("QindaQt could not restore shade after scene restart: %s",
                         qPrintable(error));
            }
        }
    };
    m_shadeMemberPlatform = std::make_unique<KWinShadeMemberPlatform>(
        m_registry, std::move(callbacks));
    m_shadeController = std::make_unique<HybridShadeController>(*m_shadeMemberPlatform);
}

bool KWinHybridSession::shadeContainer(const QString &containerId, QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        return fail(error, QStringLiteral("the selected window group is stale"));
    }
    if (m_placement->isShaded(containerId)) {
        return true;
    }
    const auto memberIds = m_runtime->topology().windowIds(containerId);
    const QString anchorId = m_groupStacking
        ? m_groupStacking->anchorMemberId(containerId) : QString{};
    if (memberIds.isEmpty() || anchorId.isEmpty()) {
        return fail(error, QStringLiteral("group has no resolvable chrome anchor to shade"));
    }
    // Captured before hiding: KWin moves focus off a member as it is hidden.
    const QString focusedMemberId =
        m_registry.windowId(KWin::workspace()->activeWindow());
    ensureShadeController();
    if (!m_shadeController->shadeMembers(containerId, memberIds, anchorId, error)) {
        return false;
    }
    if (memberIds.contains(focusedMemberId)) {
        m_shadeController->recordFocusedMember(containerId, focusedMemberId);
    }
    if (!m_placement->shade(containerId, error)) {
        QString restoreError;
        if (!m_shadeController->unshadeMembers(containerId, &restoreError)) {
            qWarning("QindaQt shade rollback could not restore members: %s",
                     qPrintable(restoreError));
        }
        return false;
    }
    synchronizeChrome();
    return true;
}

bool KWinHybridSession::unshadeContainer(const QString &containerId, QString *error)
{
    if (!m_placement->isShaded(containerId)) {
        return fail(error, QStringLiteral("group is not shaded"));
    }
    // AGENT-GUARD: restore real geometry (placement) before member content/
    // input becomes visible again, so nothing paints mid-reflow at a stale
    // frame. The reflow legitimately activates a member (which KWin unhides),
    // so enforcement is released first; a failed reflow leaves the group
    // shaded, matching HybridContainerPlacementController::unshade's contract,
    // and re-hides whatever that reflow already revealed.
    if (m_shadeController) {
        m_shadeController->beginRelease(containerId);
    }
    if (!m_placement->unshade(containerId, error)) {
        if (m_shadeController && m_shadeController->isShaded(containerId)) {
            m_shadeController->cancelRelease(containerId);
            QString reassertError;
            if (!m_shadeController->reassertContainer(containerId, &reassertError)) {
                qWarning("QindaQt failed unroll could not re-hide members: %s",
                         qPrintable(reassertError));
            }
        }
        return false;
    }
    const QString focusMemberId = m_shadeController
        ? m_shadeController->focusedMember(containerId) : QString{};
    if (m_shadeController && !m_shadeController->unshadeMembers(containerId, error)) {
        return false;
    }
    synchronizeChrome();
    // Unrolling is an explicit action on the group, like pressing its title
    // bar: hand keyboard focus back to the member that held it when the group
    // rolled up (KWin moved focus away while that member was hidden).
    auto *const focusWindow = focusMemberId.isEmpty()
        ? nullptr : m_registry.window(focusMemberId);
    if (focusWindow && !focusWindow->isDeleted() && !focusWindow->isHidden()
        && !focusWindow->isMinimized()) {
        KWin::workspace()->activateWindow(focusWindow);
    }
    return true;
}

void KWinHybridSession::applyWheelShade(const QString &containerId, bool shade)
{
    if (!ready() || !m_runtime->topology().container(containerId)
        || isContainerShaded(containerId) == shade) {
        return;
    }
    QString error;
    const bool applied = restoreMemberFocusForContainerAction(containerId, &error)
        && (shade ? shadeContainer(containerId, &error)
                  : unshadeContainer(containerId, &error));
    if (!applied) {
        qWarning("QindaQt wheel roll-up failed for '%s': %s",
                 qPrintable(containerId), qPrintable(error));
    }
}

void KWinHybridSession::forgetShadedContainer(const QString &containerId)
{
    if (!m_shadeController || !m_shadeController->isShaded(containerId)) {
        return;
    }
    // AGENT-GUARD: A container can disappear (ungroup, detach-to-singleton,
    // forget/close) while shaded. Its real committed layout was never
    // touched by shade, so no geometry restore is needed here, but member
    // paint/input eligibility must be restored before the normal teardown
    // path (independent WindowRestoreState reapplication) runs, or a
    // surviving/detached member would stay permanently hidden.
    QString error;
    if (!m_shadeController->unshadeMembers(containerId, &error)) {
        qWarning("QindaQt shade teardown could not restore members for '%s': %s",
                 qPrintable(containerId), qPrintable(error));
    }
}

void KWinHybridSession::restoreShadeForShutdown()
{
    if (!m_shadeController) {
        return;
    }
    for (const auto &containerId : m_shadeController->shadedContainerIds()) {
        forgetShadedContainer(containerId);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration

// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwiniconifyplatform.h"

#include "managedwindowregistry.h"

#include <scene/shadowitem.h>
#include <scene/windowitem.h>
#include <window.h>
#include <workspace.h>

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

// See KWinShadeMemberPlatform (kwinhybridshade.cpp): a force-visible window
// at opacity exactly 1.0 still occludes the scene beneath it with its hidden
// client's opaque region and leaves a stale ghost. Held marginally below.
constexpr qreal IconifiedWindowOpacity = 0.999;
constexpr int MaximumTransientDepth = 16;

} // namespace

KWinIconifyPlatform::KWinIconifyPlatform(ManagedWindowRegistry &registry,
                                         IconifyPlatformCallbacks callbacks)
    : m_registry(registry)
    , m_callbacks(std::move(callbacks))
{
}

KWinIconifyPlatform::~KWinIconifyPlatform()
{
    for (auto &state : m_applied) {
        disconnectAll(state);
    }
    QObject::disconnect(m_windowAdded);
}

bool KWinIconifyPlatform::hideWindow(const QString &windowId, QString *error)
{
    auto *const window = m_registry.window(windowId);
    if (!window || window->isDeleted()) {
        return fail(error, QStringLiteral("window '%1' is unavailable").arg(windowId));
    }
    auto *const item = window->windowItem();
    if (!item) {
        return fail(error, QStringLiteral("window '%1' has no scene item").arg(windowId));
    }
    auto &state = m_applied[windowId];
    state.window = window;
    // AGENT-GUARD: content, decoration, and shadow are hidden before the
    // item is forced visible and before setHidden runs, so no presented
    // frame shows client content once the roll-up has begun.
    hideContent(window, state);
    if (state.forcedItem != item) {
        item->refVisible(KWin::WindowItem::PAINT_DISABLED_BY_HIDDEN);
        state.forcedItem = item;
    }
    lowerOpacity(window, state);
    watch(windowId, window, state);
    hideTransients(windowId, window, state, 0);
    window->setHidden(true);
    updateGlobalWatch();
    return true;
}

bool KWinIconifyPlatform::showWindow(const QString &windowId, QString *)
{
    const auto found = m_applied.find(windowId);
    if (found == m_applied.end()) {
        return true;
    }
    AppliedState state = std::move(*found);
    m_applied.erase(found);
    // Disconnect first: this adapter's own setHidden(false) is no reveal.
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
            && qFuzzyCompare(window->opacity(), IconifiedWindowOpacity)) {
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

void KWinIconifyPlatform::hideContent(KWin::Window *window, AppliedState &state)
{
    auto *const item = window->windowItem();
    if (!item) {
        return;
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

void KWinIconifyPlatform::lowerOpacity(KWin::Window *window, AppliedState &state)
{
    if (window->opacity() > IconifiedWindowOpacity) {
        state.originalOpacity = window->opacity();
        window->setOpacity(IconifiedWindowOpacity);
    }
}

// AGENT-GUARD: dialogs of an iconified window would otherwise float over
// the vacated area as live, input-eligible ghosts of the rolled-up window.
void KWinIconifyPlatform::hideTransients(const QString &windowId, KWin::Window *window,
                                         AppliedState &state, int depth)
{
    const auto transients = window->transients();
    for (auto *const transient : transients) {
        if (!transient || transient->isDeleted() || depth >= MaximumTransientDepth) {
            continue;
        }
        const bool recorded = state.hiddenTransients.contains(transient);
        if (!recorded && transient->isHidden()) {
            continue;
        }
        transient->setHidden(true);
        if (!recorded) {
            state.hiddenTransients.append(transient);
            state.connections.append(watchReveal(windowId, transient));
        }
        hideTransients(windowId, transient, state, depth + 1);
    }
}

void KWinIconifyPlatform::showTransients(const AppliedState &state)
{
    for (const auto &transient : state.hiddenTransients) {
        if (transient && !transient->isDeleted()) {
            transient->setHidden(false);
        }
    }
}

// AGENT-CONTRACT: Workspace::activateWindow() calls setHidden(false) itself
// (client xdg-activation, X11 _NET_ACTIVE_WINDOW, task-list Activate,
// unminimize); for an iconified window every such reveal is a deliberate
// unroll and reaches the session through `revealed`.
QMetaObject::Connection KWinIconifyPlatform::watchReveal(const QString &windowId,
                                                         KWin::Window *window)
{
    return QObject::connect(window, &KWin::Window::hiddenChanged, window,
                            [this, windowId, window] {
        if (!window->isHidden() && m_callbacks.revealed) {
            m_callbacks.revealed(windowId);
        }
    });
}

void KWinIconifyPlatform::watch(const QString &windowId, KWin::Window *window,
                                AppliedState &state)
{
    if (!state.connections.isEmpty()) {
        return;
    }
    state.connections.append(watchReveal(windowId, window));
    state.connections.append(QObject::connect(
        window, &KWin::Window::shadowChanged, window, [this, windowId, window] {
            if (const auto found = m_applied.find(windowId); found != m_applied.end()) {
                hideContent(window, *found);
            }
        }));
    state.connections.append(QObject::connect(
        window, &KWin::Window::opacityChanged, window, [this, windowId, window] {
            if (const auto found = m_applied.find(windowId); found != m_applied.end()) {
                lowerOpacity(window, *found);
            }
        }));
    state.connections.append(QObject::connect(
        window, &KWin::Window::minimizedChanged, window, [this, windowId, window] {
            if (m_callbacks.minimizedChanged) {
                m_callbacks.minimizedChanged(windowId, window->isMinimized());
            }
        }));
    state.connections.append(QObject::connect(
        window, &KWin::Window::closed, window, [this, windowId] {
            if (const auto found = m_applied.find(windowId); found != m_applied.end()) {
                AppliedState closedState = std::move(*found);
                m_applied.erase(found);
                disconnectAll(closedState);
                showTransients(closedState);
                updateGlobalWatch();
            }
            if (m_callbacks.closed) {
                m_callbacks.closed(windowId);
            }
        }));
}

void KWinIconifyPlatform::updateGlobalWatch()
{
    const bool needed = !m_applied.isEmpty();
    if (needed && !m_windowAdded) {
        auto *const workspace = KWin::workspace();
        m_windowAdded = QObject::connect(
            workspace, &KWin::Workspace::windowAdded, workspace,
            [this](KWin::Window *added) { handleWindowAdded(added); });
    } else if (!needed && m_windowAdded) {
        QObject::disconnect(m_windowAdded);
        m_windowAdded = {};
    }
}

// A transient mapped for an iconified window is hidden with its owner and
// restored with it; it is not a reveal.
void KWinIconifyPlatform::handleWindowAdded(KWin::Window *added)
{
    int depth = 0;
    for (auto *owner = added ? added->transientFor() : nullptr;
         owner && depth < MaximumTransientDepth;
         owner = owner->transientFor(), ++depth) {
        const auto ownerId = m_registry.windowId(owner);
        const auto found = ownerId.isEmpty() ? m_applied.end() : m_applied.find(ownerId);
        if (found != m_applied.end()) {
            hideTransients(ownerId, owner, *found, 0);
            return;
        }
    }
}

void KWinIconifyPlatform::disconnectAll(AppliedState &state)
{
    for (const auto &connection : std::as_const(state.connections)) {
        QObject::disconnect(connection);
    }
    state.connections.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration

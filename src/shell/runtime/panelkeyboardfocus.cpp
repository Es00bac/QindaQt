// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelkeyboardfocus.h"

#include <LayerShellQt/Window>

#include <QQuickWindow>

namespace QindaQt::Shell {

LayerShellPanelFocusTarget::LayerShellPanelFocusTarget(WindowResolver resolver,
                                                       QObject *parent)
    : PanelFocusTarget(parent), m_resolver(std::move(resolver))
{
}

QQuickWindow *LayerShellPanelFocusTarget::resolve()
{
    QQuickWindow *window = m_resolver ? m_resolver() : nullptr;
    if (window == m_observed) {
        return window;
    }
    if (!m_observed.isNull()) {
        disconnect(m_observed, &QWindow::activeChanged, this, nullptr);
    }
    m_observed = window;
    if (window != nullptr) {
        connect(window, &QWindow::activeChanged, this,
                &PanelFocusTarget::keyboardFocusChanged);
    }
    return window;
}

void LayerShellPanelFocusTarget::grantKeyboardFocus()
{
    QQuickWindow *window = resolve();
    if (window == nullptr) {
        return;
    }
    if (auto *layer = LayerShellQt::Window::get(window)) {
        layer->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivityOnDemand);
        window->requestActivate();
    }
}

void LayerShellPanelFocusTarget::revokeKeyboardFocus()
{
    QQuickWindow *window = resolve();
    if (window == nullptr) {
        return;
    }
    if (auto *layer = LayerShellQt::Window::get(window)) {
        // AGENT-GUARD: the panel must end every open/close cycle back on None.
        // Leaving it OnDemand keeps the panel in the compositor's focus
        // rotation, and typing then lands on a bar instead of the window the
        // user is looking at.
        layer->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivityNone);
    }
}

bool LayerShellPanelFocusTarget::hasKeyboardFocus() const
{
    QQuickWindow *window = m_resolver ? m_resolver() : nullptr;
    return window != nullptr && window->isActive();
}

LauncherKeyboardFocusRelay::LauncherKeyboardFocusRelay(PanelFocusTarget &target,
                                                       QObject *parent)
    : QObject(parent), m_target(target)
{
    m_focusWait.setSingleShot(true);
    m_focusWait.setInterval(focusWaitMilliseconds());
    connect(&m_focusWait, &QTimer::timeout, this,
            &LauncherKeyboardFocusRelay::openAndStopWaiting);
    connect(&m_target, &PanelFocusTarget::keyboardFocusChanged, this, [this] {
        if (m_waiting && m_target.hasKeyboardFocus()) {
            openAndStopWaiting();
        }
    });
}

void LauncherKeyboardFocusRelay::requestOpen()
{
    if (m_waiting) {
        return;
    }
    if (!m_holdingGrant) {
        m_target.grantKeyboardFocus();
        m_holdingGrant = true;
    }
    if (m_target.hasKeyboardFocus()) {
        // The panel already holds focus (a second press while the browser is
        // open, or a compositor that granted it immediately).
        Q_EMIT openNow();
        return;
    }
    m_waiting = true;
    m_focusWait.start();
}

void LauncherKeyboardFocusRelay::browserClosed()
{
    m_waiting = false;
    m_focusWait.stop();
    if (m_holdingGrant) {
        m_target.revokeKeyboardFocus();
        m_holdingGrant = false;
    }
}

void LauncherKeyboardFocusRelay::openAndStopWaiting()
{
    m_waiting = false;
    m_focusWait.stop();
    Q_EMIT openNow();
}

} // namespace QindaQt::Shell

// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_overlay_surface.h"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

namespace QindaQt::Apps::PolkitAgent {
namespace {

QScreen *screenUnderPointer()
{
    // screenAt returns null where the platform will not report a global
    // cursor position (GatherOverviewComposition::screenToOpenOn() records
    // the same Wayland limitation), which is why the primary screen is the
    // fallback rather than the assumption.
    if (QScreen *under = QGuiApplication::screenAt(QCursor::pos())) {
        return under;
    }
    return QGuiApplication::primaryScreen();
}

} // namespace

void PolkitOverlaySurface::configure(QQuickWindow &window)
{
    QScreen *screen = screenUnderPointer();
    window.setFlag(Qt::FramelessWindowHint, true);
    window.setColor(Qt::transparent);
    if (screen != nullptr) {
        window.setScreen(screen);
        window.setGeometry(screen->geometry());
    }
    auto *layerWindow = LayerShellQt::Window::get(&window);
    if (layerWindow == nullptr) {
        return;
    }
    if (screen != nullptr) {
        layerWindow->setScreen(screen);
    }
    layerWindow->setScope(QStringLiteral("polkit-agent"));
    // AGENT-GUARD: exclusive keyboard interactivity is the entire point of
    // this surface -- a password must never be typeable into whatever window
    // happened to have focus underneath it.
    layerWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityExclusive);
    layerWindow->setActivateOnShow(true);
    layerWindow->setCloseOnDismissed(false);
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorBottom;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    layerWindow->setAnchors(anchors);
    layerWindow->setExclusiveZone(-1);
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
}

} // namespace QindaQt::Apps::PolkitAgent

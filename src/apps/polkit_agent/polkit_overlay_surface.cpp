// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_overlay_surface.h"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

namespace QindaQt::Apps::PolkitAgent {
namespace {

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

QScreen *screenUnderPointer()
{
    // AGENT-NOTE: QCursor::pos may return (0, 0) on Wayland when global
    // pointer position is unavailable. This mirrors GatherOverviewComposition::
    // screenToOpenOn(); screenAt can then choose the first output, so multi-
    // output pointer placement remains a live-session qualification boundary.
    if (QScreen *under = QGuiApplication::screenAt(QCursor::pos())) {
        return under;
    }
    return QGuiApplication::primaryScreen();
}

bool configureLayerWindow(QQuickWindow &window,
                          LayerShellQt::Window *layerWindow,
                          QString *error)
{
    if (layerWindow == nullptr) {
        setError(error, QStringLiteral(
            "LayerShellQt did not create a layer surface; refusing to show an ordinary authentication window"));
        return false;
    }

    QScreen *screen = screenUnderPointer();
    window.setFlag(Qt::FramelessWindowHint, true);
    window.setColor(Qt::transparent);
    if (screen != nullptr) {
        window.setScreen(screen);
        window.setGeometry(screen->geometry());
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
    setError(error, {});
    return true;
}

} // namespace

bool PolkitOverlaySurface::configure(QQuickWindow &window, QString *error)
{
    // LayerShellQt installs an attached Window object even for Qt offscreen;
    // only the Wayland platform can create a keyboard-exclusive protocol role.
    if (QGuiApplication::platformName() != QLatin1String("wayland")) {
        setError(error, QStringLiteral(
            "Wayland layer-shell is required; refusing to show an ordinary authentication window"));
        return false;
    }
    return configureLayerWindow(window, LayerShellQt::Window::get(&window), error);
}

} // namespace QindaQt::Apps::PolkitAgent

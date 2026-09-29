// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/authentication_overlay/overlay_surface.h>
#include <LayerShellQt/Shell>
#include <QThread>

#include <LayerShellQt/Window>

#include <QCursor>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

namespace QindaQt::AuthenticationOverlay {
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
                          const QString &scope, QString *error)
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
    layerWindow->setScope(scope);
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

bool OverlaySurface::initializePlatform() {
    if (QGuiApplication::instance()) return false;
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    LayerShellQt::Shell::useLayerShell();
    QT_WARNING_POP
    return true;
}
bool OverlaySurface::configure(QQuickWindow &window, const QString &scope, QString *error)
{
    if (window.thread() != QThread::currentThread() || window.isVisible() || window.handle()
        || scope.isEmpty() || scope.size() > 64) {
        setError(error, QStringLiteral("Authentication role must be configured before native creation/show"));
        return false;
    }
    // LayerShellQt installs an attached Window object even for Qt offscreen;
    // only the Wayland platform can create a keyboard-exclusive protocol role.
    if (QGuiApplication::platformName() != QLatin1String("wayland")) {
        setError(error, QStringLiteral(
            "Wayland layer-shell is required; refusing to show an ordinary authentication window"));
        return false;
    }
    return configureLayerWindow(window, LayerShellQt::Window::get(&window), scope, error);
}

} // namespace QindaQt::AuthenticationOverlay

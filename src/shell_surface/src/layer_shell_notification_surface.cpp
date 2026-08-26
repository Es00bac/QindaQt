// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/layer_shell_notification_surface.h"

#include <LayerShellQt/Window>

#include <QQuickWindow>
#include <QScreen>

#include <utility>

namespace QindaQt::ShellSurface {
namespace {

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

bool validSize(const QSize &size)
{
    return size.width() >= 240 && size.width() <= 1'024 && size.height() >= 1 &&
        size.height() <= 2'048;
}

bool validMargins(const QMargins &margins)
{
    return margins.left() >= 0 && margins.top() >= 0 && margins.right() >= 0 &&
        margins.bottom() >= 0 && margins.left() <= 4'096 &&
        margins.top() <= 4'096 && margins.right() <= 4'096 &&
        margins.bottom() <= 4'096;
}

} // namespace

bool LayerShellNotificationSurface::configure(
    QQuickWindow &window, QScreen &screen, NotificationSurfaceRole role,
    const QSize &desiredSize, const QMargins &margins, QString *error)
{
    if (window.isVisible() || window.parent() != nullptr ||
        window.transientParent() != nullptr || !validSize(desiredSize) ||
        !validMargins(margins)) {
        setError(error,
                 QStringLiteral("notification surface configuration is invalid"));
        return false;
    }
    window.setFlag(Qt::FramelessWindowHint, true);
    window.setColor(Qt::transparent);
    window.setScreen(&screen);
    window.resize(desiredSize);
    auto *layerWindow = LayerShellQt::Window::get(&window);
    if (layerWindow == nullptr) {
        setError(error, QStringLiteral("LayerShellQt rejected notification surface"));
        return false;
    }
    layerWindow->setWantsToBeOnActiveScreen(false);
    layerWindow->setScreen(&screen);
    layerWindow->setScope(role == NotificationSurfaceRole::PopupStack
                              ? QStringLiteral("notification-popup")
                              : QStringLiteral("notification-center"));
    layerWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityOnDemand);
    layerWindow->setActivateOnShow(false);
    layerWindow->setCloseOnDismissed(false);
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorRight;
    layerWindow->setAnchors(anchors);
    layerWindow->setMargins(margins);
    layerWindow->setDesiredSize(desiredSize);
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow->setExclusiveEdge(LayerShellQt::Window::AnchorNone);
    layerWindow->setExclusiveZone(0);
    setError(error, {});
    return true;
}

bool LayerShellNotificationSurface::resize(
    QQuickWindow &window, const QSize &desiredSize, QString *error)
{
    if (!validSize(desiredSize)) {
        setError(error, QStringLiteral("notification surface size is invalid"));
        return false;
    }
    auto *layerWindow = LayerShellQt::Window::get(&window);
    if (layerWindow == nullptr) {
        setError(error, QStringLiteral("notification surface has no layer-shell role"));
        return false;
    }
    window.resize(desiredSize);
    layerWindow->setDesiredSize(desiredSize);
    setError(error, {});
    return true;
}

} // namespace QindaQt::ShellSurface

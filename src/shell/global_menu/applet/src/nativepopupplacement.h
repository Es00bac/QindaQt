// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QRectF>

// QWindow must be complete: the moc-generated metatype for the
// configurePopupWindow() signature requires it.
#include <QtGui/QWindow>

#include <QtQml/qqmlregistration.h>

namespace QindaQt::Shell::GlobalMenu
{

// AGENT-CONTRACT (QtWayland faux positioner API): on Wayland a Popup.Window's
// position is server-side — QQuickPopupPositioner::repositionPopupWindow()
// returns early and popup x/y never reach the compositor. QtWayland's
// QWaylandXdgSurface::createPositioner() instead anchors the xdg_popup from
// the popup window's dynamic "_q_waylandPopupAnchor*" properties, read once
// when the platform surface is created. Those must be REAL QObject dynamic
// properties: assigning an unknown name on a C++-created QObject wrapper from
// QML only creates a JavaScript expando on the wrapper (QV4::
// QObjectWrapper::virtualPut falls through to Object::virtualPut), which is
// invisible to QtWayland and lost when the wrapper is re-created. This
// singleton is the owned bridge that performs QObject::setProperty.
// Stateless; one instance per engine, GUI thread like every QML singleton.
class NativePopupPlacement final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit NativePopupPlacement(QObject *parent = nullptr);

    // anchorSceneRect is the anchor's rectangle in the transient-parent
    // (panel) window's coordinate system — the caller maps with
    // anchorItem.mapToItem(null, 0, 0) plus the anchor's size. The dropdown
    // contract mirrors Qt's Menu window type: below the anchor rect,
    // left-aligned; slide_x|slide_y|flip_y keeps the popup on screen and
    // flips it above the anchor at the screen's bottom edge. Fail-closed: a
    // null window or invalid rect writes nothing.
    Q_INVOKABLE void configurePopupWindow(QWindow *window,
                                          const QRectF &anchorSceneRect) const;
};

} // namespace QindaQt::Shell::GlobalMenu

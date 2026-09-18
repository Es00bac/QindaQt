// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick

// A finger or stylus held on something asks for exactly the menu a right
// click would (ADR-0193). Place it over the same area as the pointer handler:
// it takes no grab until the long press completes, so taps and drags under it
// keep working, and it never fires for a mouse (mice have a right button).
Item {
    id: root

    // Seconds a finger holds still before the menu opens; the compositor's
    // chrome uses the same half second.
    property real longPressSeconds: 0.5
    // The held position in this item's coordinates.
    signal contextRequested(point position)

    TapHandler {
        id: touchHold
        objectName: "touchContextTapHandler"
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        longPressThreshold: root.longPressSeconds
        onLongPressed: root.contextRequested(Qt.point(point.position.x, point.position.y))
    }
}

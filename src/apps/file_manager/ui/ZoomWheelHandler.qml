// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Emits intent only; the window owns the bounded size shared by both views.
WheelHandler {
    id: handler
    target: null
    acceptedModifiers: Qt.ControlModifier
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    property real remainder: 0
    signal zoomRequested(int steps)

    onWheel: (event) => {
        // AGENT-NOTE: Angle deltas are eighth-degrees (120 per detent).
        // Preserve sub-detent input instead of rounding each high-resolution
        // event up to a full zoom. Pixel-only devices use 40px per step.
        remainder += event.angleDelta.y !== 0 ? event.angleDelta.y / 120
                                               : event.pixelDelta.y / 40
        const steps = Math.trunc(remainder)
        remainder -= steps
        if (steps !== 0) zoomRequested(steps)
        event.accepted = true
    }
}

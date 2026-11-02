// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Placeholder for the Touchpad feature. AGENT-NOTE: self-contained on purpose; the lane that builds
// Touchpad replaces this whole file and keeps the `openPage`/`backRequested` contract below.
ScreenFrame {
    title: qsTr("Touchpad")
    backVisible: true

    Tk.EmptyState {
        objectName: "placeholder"
        iconName: "mouse-pointer-2"
        title: qsTr("Coming in a later update")
        text: qsTr("Use the phone as a touchpad for the desktop pointer.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
}

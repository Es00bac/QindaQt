// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Placeholder for the Keyboard feature. AGENT-NOTE: self-contained on purpose; the lane that builds
// Keyboard replaces this whole file and keeps the `openPage`/`backRequested` contract below.
ScreenFrame {
    title: qsTr("Keyboard")
    backVisible: true

    Tk.EmptyState {
        objectName: "placeholder"
        iconName: "keyboard"
        title: qsTr("Coming in a later update")
        text: qsTr("Type on the phone and have it land in the focused desktop window.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
}

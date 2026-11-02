// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Placeholder for the Vault feature. AGENT-NOTE: self-contained on purpose; the lane that builds
// Vault replaces this whole file and keeps the `openPage`/`backRequested` contract below.
ScreenFrame {
    title: qsTr("Vault")
    backVisible: true

    Tk.EmptyState {
        objectName: "placeholder"
        iconName: "lock"
        title: qsTr("Coming in a later update")
        text: qsTr("Keep passwords on the phone and type them into the desktop on request.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
}

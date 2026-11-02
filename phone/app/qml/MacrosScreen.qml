// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Placeholder for the Macros feature. AGENT-NOTE: self-contained on purpose; the lane that builds
// Macros replaces this whole file and keeps the `openPage`/`backRequested` contract below.
ScreenFrame {
    title: qsTr("Macros")
    backVisible: true

    Tk.EmptyState {
        objectName: "placeholder"
        iconName: "zap"
        title: qsTr("Coming in a later update")
        text: qsTr("Record and run your own key sequences on the desktop.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
}

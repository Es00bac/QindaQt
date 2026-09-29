// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// ADR-0286: every saved per-display and per-desktop wallpaper in the draft,
// including choices for displays that are not connected and desktops that no
// longer exist. Those keep their saved choice (it applies again when the
// display returns), so the list is where they can be seen and removed.
ColumnLayout {
    id: list

    required property var rows
    required property bool editable
    signal removeRequested(string display, string desktop)

    visible: list.rows.length > 0
    spacing: Tokens.space["1"]

    Label {
        Layout.fillWidth: true
        text: qsTr("Separate wallpapers")
        font.weight: Font.DemiBold
        Accessible.role: Accessible.Heading
    }

    Repeater {
        model: list.rows

        delegate: RowLayout {
            id: row
            required property var modelData
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Label {
                id: rowText
                Layout.fillWidth: true
                // Absent displays and desktops are named in words, not only
                // dimmed, so the state never depends on colour.
                muted: !row.modelData.displayPresent || !row.modelData.desktopPresent
                text: qsTr("%1, %2: %3").arg(row.modelData.displayLabel)
                          .arg(row.modelData.desktopLabel).arg(row.modelData.wallpaperLabel)
            }

            Button {
                objectName: "appearanceWallpaperRemoveChoice"
                emphasized: false
                available: list.editable
                text: qsTr("Remove")
                accessibleDescription: qsTr("Stop using a separate wallpaper for %1")
                                       .arg(rowText.text)
                onClicked: list.removeRequested(row.modelData.display, row.modelData.desktop)
            }
        }
    }
}

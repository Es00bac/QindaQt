// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The wallpaper grid: "No wallpaper", the bundled images, then the user's
// gallery. It reports picks and highlights the chosen value; the section
// decides which scope a pick edits (ADR-0279, ADR-0286).
Flow {
    id: gallery

    // {name, value, previewUrl} entries, bundled first.
    required property var wallpapers
    // The value chosen for the current scope; null when the scope has no
    // choice of its own, so nothing is highlighted.
    required property var pickedValue
    required property bool editable
    signal picked(string value)
    readonly property Item firstChoice: noWallpaperButton

    spacing: Tokens.space["2"]

    Button {
        id: noWallpaperButton
        objectName: "noWallpaperButton"
        width: 144
        height: 94
        text: qsTr("No wallpaper")
        emphasized: gallery.pickedValue === ""
        available: gallery.editable
        onClicked: gallery.picked("")
    }

    Repeater {
        model: gallery.wallpapers

        Button {
            id: wallpaperButton
            required property var modelData
            objectName: "bundledWallpaperButton"
            width: 144
            height: 94
            text: modelData.name
            emphasized: gallery.pickedValue === modelData.value
            available: gallery.editable
            accessibleDescription: qsTr("Select this wallpaper")
            onClicked: gallery.picked(modelData.value)

            contentItem: ColumnLayout {
                spacing: Tokens.space["1"]
                Image {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    source: wallpaperButton.modelData.previewUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    Accessible.ignored: true
                }
                Label {
                    Layout.fillWidth: true
                    text: wallpaperButton.modelData.name
                    color: !wallpaperButton.enabled ? Tokens.fg.muted
                         : wallpaperButton.emphasized ? Tokens.accent.fg
                                                      : Tokens.fg.default
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    Accessible.ignored: true
                }
            }
        }
    }
}

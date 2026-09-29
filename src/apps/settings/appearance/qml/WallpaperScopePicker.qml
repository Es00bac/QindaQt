// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// ADR-0286: chooses where the next wallpaper pick applies — every display or
// one display (a miniature of the arrangement), and every desktop or one
// virtual desktop. Selecting a scope never edits the draft; the section's
// gallery does. The route model owns what a choice means and how scopes
// inherit; this only reports the selection and shows the model's answer.
ColumnLayout {
    id: picker

    required property var targets
    required property string selectedDisplay
    required property string selectedDesktop
    required property var choice
    required property bool editable
    signal displayPicked(string stableId)
    signal desktopPicked(string desktopId)
    signal clearRequested()

    readonly property var displays: picker.targets?.displays ?? []
    readonly property var desktops: picker.targets?.desktops ?? []
    readonly property var desktopChoices: [{ id: "", name: qsTr("All desktops") }]
                                          .concat(picker.desktops)
    readonly property Item firstFocusTarget: allDisplaysButton
    readonly property var bounds: {
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        for (const display of picker.displays) {
            left = Math.min(left, display.x)
            top = Math.min(top, display.y)
            right = Math.max(right, display.x + display.width)
            bottom = Math.max(bottom, display.y + display.height)
        }
        return picker.displays.length > 0
            ? { x: left, y: top, width: Math.max(1, right - left), height: Math.max(1, bottom - top) }
            : null
    }

    spacing: Tokens.space["2"]

    function displayTitle(stableId) {
        const display = picker.displays.find(entry => entry.stableId === stableId)
        return display === undefined ? qsTr("This display") : display.title
    }

    function scopeName() {
        const desktop = picker.desktops.find(entry => entry.id === picker.selectedDesktop)
        const desktopName = desktop === undefined ? "" : desktop.name
        if (picker.selectedDisplay === "")
            return desktopName === "" ? qsTr("Every display") : qsTr("%1 on every display").arg(desktopName)
        const displayName = picker.displayTitle(picker.selectedDisplay)
        return desktopName === "" ? displayName : qsTr("%1 on %2").arg(desktopName).arg(displayName)
    }

    function sourceName(scope) {
        return scope === "display" ? qsTr("the choice for %1").arg(picker.displayTitle(picker.selectedDisplay))
             : scope === "desktop" ? qsTr("the choice for this desktop on every display")
             : qsTr("the wallpaper for every display")
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Show on")
        font.weight: Font.DemiBold
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Button {
            id: allDisplaysButton
            objectName: "appearanceWallpaperAllDisplays"
            text: qsTr("All displays")
            emphasized: picker.selectedDisplay === ""
            available: picker.editable
            Accessible.role: Accessible.RadioButton
            Accessible.checkable: true
            Accessible.checked: picker.selectedDisplay === ""
            accessibleDescription: qsTr("Choose the wallpaper every display shows")
            onClicked: picker.displayPicked("")
        }

        // Presentation-only miniature: the Display route owns arrangement.
        Item {
            id: arrangement
            objectName: "appearanceWallpaperDisplayMap"
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            visible: picker.displays.length > 0
            readonly property real fit: picker.bounds === null ? 0
                : Math.min(width / picker.bounds.width, height / picker.bounds.height)
            readonly property real offsetX: (width - (picker.bounds?.width ?? 0) * fit) / 2
            readonly property real offsetY: (height - (picker.bounds?.height ?? 0) * fit) / 2

            Repeater {
                model: picker.displays

                delegate: Button {
                    id: tile
                    required property var modelData
                    readonly property bool chosen: picker.selectedDisplay === tile.modelData.stableId
                    objectName: "appearanceWallpaperDisplay_" + tile.modelData.stableId
                    x: arrangement.offsetX + (tile.modelData.x - (picker.bounds?.x ?? 0)) * arrangement.fit
                    y: arrangement.offsetY + (tile.modelData.y - (picker.bounds?.y ?? 0)) * arrangement.fit
                    width: Math.max(Tokens.space["5"], tile.modelData.width * arrangement.fit - 2)
                    height: Math.max(Tokens.space["5"], tile.modelData.height * arrangement.fit - 2)
                    leftPadding: Tokens.space["1"]
                    rightPadding: Tokens.space["1"]
                    topPadding: Tokens.space["1"]
                    bottomPadding: Tokens.space["1"]
                    text: String(tile.modelData.ordinal)
                    emphasized: tile.chosen
                    available: picker.editable && tile.modelData.assignable
                    Accessible.role: Accessible.RadioButton
                    Accessible.checkable: true
                    Accessible.checked: tile.chosen
                    Accessible.name: qsTr("%1, %2%3").arg(tile.modelData.title).arg(tile.modelData.name)
                                     .arg(tile.modelData.primary ? qsTr(", primary") : "")
                    accessibleDescription: tile.modelData.assignable
                        ? qsTr("Choose the wallpaper this display shows")
                        : qsTr("Identical displays cannot be told apart; use All displays")
                    onClicked: picker.displayPicked(tile.modelData.stableId)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: picker.desktops.length > 0
        spacing: Tokens.space["2"]

        Label {
            text: qsTr("Desktop")
            font.weight: Font.DemiBold
        }

        ComboBox {
            id: desktopSelector
            objectName: "appearanceWallpaperDesktopSelector"
            Layout.preferredWidth: 240
            enabled: picker.editable
            model: picker.desktopChoices
            textRole: "name"
            currentIndex: Math.max(0, picker.desktopChoices.findIndex(
                                       entry => entry.id === picker.selectedDesktop))
            Accessible.name: qsTr("Desktop")
            accessibleDescription: qsTr("Choose a virtual desktop to give its own wallpaper")
            onActivated: index => picker.desktopPicked(picker.desktopChoices[index].id)
        }
    }

    Label {
        objectName: "appearanceWallpaperScopeSummary"
        Layout.fillWidth: true
        visible: picker.choice !== null
        muted: picker.choice !== null && !picker.choice.explicit
        text: picker.choice === null ? ""
            : picker.choice.explicit
              ? qsTr("%1: %2").arg(picker.scopeName()).arg(picker.choice.label)
              : qsTr("%1 uses %2: %3").arg(picker.scopeName())
                    .arg(picker.sourceName(picker.choice.scope)).arg(picker.choice.label)
    }

    Button {
        objectName: "appearanceWallpaperFollowButton"
        visible: picker.choice !== null && picker.choice.explicit
                 && (picker.selectedDisplay !== "" || picker.selectedDesktop !== "")
        emphasized: false
        available: picker.editable
        text: qsTr("Stop using a separate wallpaper here")
        accessibleDescription: qsTr("%1 goes back to the wallpaper it would otherwise show")
                               .arg(picker.scopeName())
        onClicked: picker.clearRequested()
    }
}

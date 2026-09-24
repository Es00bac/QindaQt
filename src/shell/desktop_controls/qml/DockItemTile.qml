// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// One dock tile (ADR-0265): an application, a folder, a file, the Trash, or
// a group drawn as a plate of its first member icons. The tile owns only its
// presentation; activation, menus, and every drag decision belong to
// QuickLaunchApplet, which re-enters the dock facade.
//
// AGENT-GUARD: drag, gap, and magnification visuals are transforms only.
// The tile's layout size and hit target never change while the pointer
// moves, so GridLayout and the panel's dock geometry stay exact.
T.Button {
    id: tile

    required property var row
    required property int visualIndex
    property bool vertical: false
    property bool dockMode: false
    property int tileExtent: 60
    property int iconExtent: 40
    property bool reducedMotion: false
    property bool luna: false
    property bool launchEnabled: true
    // Magnification factor from the strip (1.0 = rest).
    property real zoomScale: 1.0
    // Main-axis translation: the dragged tile follows the pointer, the others
    // open or close the insertion gap.
    property real mainShift: 0
    property bool dragHeld: false
    property bool mergeTarget: false
    property bool removing: false

    signal activated()
    signal contextRequested()

    readonly property string kind: String(row.kind ?? "application")
    readonly property string label: kind === "trash" ? qsTr("Trash")
                                                     : String(row.displayText ?? "")
    readonly property bool running: Boolean(row.running)
    readonly property var previewIcons: row.previewIcons ?? []

    function kindDescription() {
        switch (kind) {
        case "group": return qsTr("Group of %1 applications").arg((row.members ?? []).length)
        case "folder": return qsTr("Folder")
        case "file": return qsTr("File")
        case "trash": return qsTr("Trash")
        default: return String(row.accessibleDescription ?? "")
        }
    }

    objectName: "quickLaunchEntry"
    // The tile owns the entire hover envelope, so magnification stays inside
    // its hit target and never asks GridLayout to grow after the pointer moves.
    Layout.preferredWidth: tileExtent
    Layout.preferredHeight: tileExtent
    padding: dockMode ? 0 : Tokens.space["2"]
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    enabled: launchEnabled || kind !== "application"
    opacity: removing ? 0.45 : 1.0
    z: dragHeld ? 2 : 0
    transform: Translate {
        x: tile.vertical ? 0 : tile.mainShift
        y: tile.vertical ? tile.mainShift : 0
    }

    // Running state is words for assistive technology, never the dot alone.
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.description: running ? qsTr("Running. %1").arg(kindDescription())
                                     : kindDescription()

    T.ToolTip {
        objectName: "quickLaunchEntryTooltip"
        visible: tile.dockMode && tile.hovered && !tile.dragHeld
        text: tile.label
        delay: Tokens.motion.short
        popupType: T.Popup.Window
    }

    onClicked: activated()
    Keys.onReturnPressed: activated()
    Keys.onEnterPressed: activated()
    Accessible.onPressAction: activated()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Menu
                || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
            tile.contextRequested()
            event.accepted = true
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: tile.contextRequested()
    }

    contentItem: Item {
        id: face

        // Bottom-anchored swell inside the tile's reserved envelope; the
        // hovered tile is the falloff peak, so the zoom subsumes the former
        // flat hover bump.
        readonly property real swell: !tile.dockMode
            ? (tile.hovered && !tile.reducedMotion ? 1.08 : 1.0)
            : (tile.reducedMotion ? 1.0 : Math.max(1.0, tile.zoomScale))
        property real hoverLift: tile.dockMode && tile.hovered && !tile.reducedMotion ? -3 : 0

        // Tokens clamp motion durations for reduced-motion accessibility;
        // the tile owns no parallel preference.
        Behavior on hoverLift {
            NumberAnimation { duration: Tokens.motion.short }
        }

        ShellIcons.Icon {
            id: entryIcon
            objectName: "quickLaunchEntryIcon"
            visible: tile.kind !== "group"
            width: size
            height: size
            anchors.centerIn: parent
            name: String(tile.row.iconName ?? "")
            size: tile.iconExtent
            color: tile.enabled ? Tokens.fg.default : Tokens.fg.disabled
            symbolic: false
            fallbackText: tile.label
            transformOrigin: Item.Bottom
            scale: face.swell
            transform: Translate { y: face.hoverLift }
            Accessible.ignored: true

            Behavior on scale {
                NumberAnimation { duration: Tokens.motion.short }
            }
        }

        // A group is a plate showing its first few applications.
        Rectangle {
            id: groupPlate
            objectName: "quickLaunchGroupPlate"
            visible: tile.kind === "group"
            width: tile.iconExtent
            height: tile.iconExtent
            anchors.centerIn: parent
            radius: Tokens.radius.m
            color: Tokens.bg.raised
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.divider
            transformOrigin: Item.Bottom
            scale: face.swell
            transform: Translate { y: face.hoverLift }
            Accessible.ignored: true

            Behavior on scale {
                NumberAnimation { duration: Tokens.motion.short }
            }

            Grid {
                anchors.centerIn: parent
                columns: 2
                spacing: 2

                Repeater {
                    model: tile.previewIcons

                    ShellIcons.Icon {
                        required property var modelData
                        name: String(modelData)
                        size: Math.max(8, Math.floor((tile.iconExtent - 8) / 2))
                        width: size
                        height: size
                        symbolic: false
                        fallbackText: ""
                        Accessible.ignored: true
                    }
                }
            }
        }

        // Running: a dot on the panel-facing edge (the accessible description
        // says it in words).
        Rectangle {
            objectName: "quickLaunchRunningIndicator"
            visible: tile.running
            width: 4
            height: 4
            radius: 2
            anchors.horizontalCenter: tile.vertical ? undefined : parent.horizontalCenter
            anchors.bottom: tile.vertical ? undefined : parent.bottom
            anchors.bottomMargin: tile.dockMode ? Tokens.space["1"] : 0
            anchors.verticalCenter: tile.vertical ? parent.verticalCenter : undefined
            anchors.left: tile.vertical ? parent.left : undefined
            color: Tokens.fg.default
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: tile.dockMode ? Tokens.radius.l : Tokens.radius.m
        // Luna tiles keep a translucent white hover so the state stays
        // visible on the dark taskbar gradient (ADR-0124).
        color: tile.mergeTarget ? Tokens.state.hover
             : tile.down ? (tile.luna ? "#33518f" : Tokens.state.pressed)
             : tile.hovered ? (tile.luna ? "#3d6cb8" : Tokens.state.hover)
             : "transparent"
        // A drop onto this tile groups; the ring says so in shape as well as
        // colour, and the drop target never relies on colour alone.
        border.width: tile.mergeTarget ? Tokens.space["1"] : 0
        border.color: Tokens.accent.default
        C.FocusRing { anchors.fill: parent; control: tile }
    }
}

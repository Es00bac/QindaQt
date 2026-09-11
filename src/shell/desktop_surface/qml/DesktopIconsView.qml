// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Desktop-icons applet presentation (ADR-0125): the places facade rendered as
// selectable icon tiles. Windows-style placement fills one top-down column at
// the chosen edge; `right` anchors the block to the output's right edge.
// Icon names come from the places rows (PlacesController iconName); the one
// literal below is the icon-first fallback for a row without a name.
Item {
    id: root

    required property var settings
    // Borrowed DesktopControlsAccess facade; may be null. Only the places
    // sub-facade is consumed (rows + open), never the root object.
    required property var access

    readonly property bool placementRight: {
        const requested = settings && settings.placement !== undefined
                          ? String(settings.placement) : "left"
        return requested === "right"
    }
    readonly property int iconSize: {
        const requested = settings && settings.iconSize !== undefined
                          ? Number(settings.iconSize) : 48
        if (Number.isNaN(requested)) {
            return 48
        }
        return Math.round(Math.min(96, Math.max(24, requested)))
    }
    // Single selection: exactly one tile may be highlighted at a time.
    property string selectedId: ""

    // AGENT-GUARD: the revision reference forces this binding to re-evaluate
    // when reflow() bumps it. PlacesController.rows is CONSTANT, so without
    // the reference "Arrange Icons"/"Refresh"/"Sort By"/"Clean Up" would
    // silently do nothing after startup.
    property int revision: 0
    readonly property var rows: {
        const currentRevision = revision
        if (access === null || access.places === null) {
            return []
        }
        return access.places.rows
    }

    implicitWidth: flow.implicitWidth + flow.anchors.leftMargin
                   + flow.anchors.rightMargin
    implicitHeight: flow.implicitHeight + flow.anchors.topMargin

    function clearSelection() {
        selectedId = ""
    }

    // Re-sorts and re-queries the grid: rows are re-read from the places
    // facade and the column flow restarts. Every menu re-sort entry lands
    // here.
    function reflow() {
        selectedId = ""
        revision += 1
    }

    function openPlace(placeId) {
        if (access !== null && access.places !== null) {
            access.places.open(placeId)
        }
    }

    Flow {
        id: flow
        objectName: "desktopIconsFlow"

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 6
        anchors.left: root.placementRight ? undefined : parent.left
        anchors.right: root.placementRight ? parent.right : undefined
        flow: Flow.TopToBottom
        spacing: 4

        Repeater {
            model: root.rows

            Rectangle {
                id: tile
                objectName: "desktopIconsTile"

                required property var modelData
                required property int index

                readonly property string placeId: String(modelData.id)
                readonly property string placeName: String(modelData.label)
                readonly property bool selected:
                    root.selectedId === placeId

                width: Math.max(root.iconSize + 24, label.implicitWidth + 12)
                height: root.iconSize + 40
                radius: 4
                color: selected ? "#33ffffff"
                    : tileInput.containsMouse ? "#22ffffff" : "transparent"

                Keys.onReturnPressed: root.openPlace(placeId)
                Keys.onEnterPressed: root.openPlace(placeId)

                MouseArea {
                    id: tileInput
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.selectedId = tile.placeId
                        tile.forceActiveFocus(Qt.MouseFocusReason)
                    }
                    onDoubleClicked: root.openPlace(tile.placeId)
                }

                ShellIcons.Icon {
                    id: icon
                    objectName: "desktopIconsTileIcon"
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: String(tile.modelData.iconName) !== ""
                          ? String(tile.modelData.iconName) : "folder"
                    size: root.iconSize
                    fallbackText: tile.placeName
                    Accessible.ignored: true
                }

                Text {
                    id: label
                    objectName: "desktopIconsTileLabel"
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 8
                    text: tile.placeName
                    color: "#ffffff"
                    // Subtle dark shadow keeps white readable on bright
                    // wallpapers.
                    style: Text.Raised
                    styleColor: "#80000000"
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                    font.pixelSize: 12
                    Accessible.ignored: true
                }

                Accessible.role: Accessible.Button
                Accessible.name: String(tile.modelData.accessibleName)
                Accessible.selected: tile.selected
                Accessible.onPressAction: root.openPlace(tile.placeId)
            }
        }
    }
}

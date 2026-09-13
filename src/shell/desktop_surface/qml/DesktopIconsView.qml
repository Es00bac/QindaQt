// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Desktop-icons applet presentation (ADR-0125): the Desktop-directory
// contents rendered as selectable icon tiles. Windows-style placement fills
// one top-down column at the chosen edge; `right` anchors the block to the
// output's right edge. Icon names come from the content rows
// (DesktopContentsController iconName); the one literal below is the
// icon-first fallback for a row without a name.
Item {
    id: root

    required property var settings
    // Owned DesktopContentsController instance (never null): the real
    // Desktop-directory listing plus the bounded open() dispatch.
    required property var contents

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

    // contents.rows is NOTIFY'd, so binding to it directly re-evaluates the
    // flow whenever refresh() publishes a new listing; no revision counter
    // needed.
    readonly property var rows: root.contents.rows

    implicitWidth: flow.implicitWidth + flow.anchors.leftMargin
                   + flow.anchors.rightMargin
    implicitHeight: flow.implicitHeight + flow.anchors.topMargin

    function clearSelection() {
        selectedId = ""
    }

    // Re-lists the Desktop directory through the boundary and restarts the
    // column flow. Every menu re-sort/refresh/new-folder entry lands here.
    function reflow() {
        selectedId = ""
        root.contents.refresh()
    }

    function openEntry(entryId) {
        root.contents.open(entryId)
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

                readonly property string entryId: String(modelData.id)
                readonly property string entryLabel: String(modelData.label)
                readonly property bool selected:
                    root.selectedId === entryId

                width: Math.max(root.iconSize + 24, label.implicitWidth + 12)
                height: root.iconSize + 40
                radius: 4
                color: selected ? "#33ffffff"
                    : tileInput.containsMouse ? "#22ffffff" : "transparent"

                Keys.onReturnPressed: root.openEntry(entryId)
                Keys.onEnterPressed: root.openEntry(entryId)

                MouseArea {
                    id: tileInput
                    anchors.fill: parent
                    hoverEnabled: true
                    // AGENT-CONTRACT: Qt.MiddleButton is claimed here as a
                    // deliberate no-op (see DesktopSurface.qml's
                    // desktopSurfaceInput) so a middle click over a tile
                    // never activates it and never falls through to reopen
                    // the Applications popup underneath — tile middle click
                    // must stay inert.
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: (mouse) => {
                        if (mouse.button !== Qt.LeftButton) {
                            return
                        }
                        root.selectedId = tile.entryId
                        tile.forceActiveFocus(Qt.MouseFocusReason)
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.button !== Qt.LeftButton) {
                            return
                        }
                        root.openEntry(tile.entryId)
                    }
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
                    fallbackText: tile.entryLabel
                    Accessible.ignored: true
                }

                Text {
                    id: label
                    objectName: "desktopIconsTileLabel"
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 8
                    text: tile.entryLabel
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
                Accessible.onPressAction: root.openEntry(tile.entryId)
            }
        }
    }
}

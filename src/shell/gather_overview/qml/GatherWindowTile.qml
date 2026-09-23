// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// One free window as a grid tile: not iconified, and not a member of any
// container. A bounded ScreenShot2 capture fills the thumbnail area when it
// arrives; the icon is the fallback for denied, stale or unavailable captures
// (ADR-0241). The planner still owns the cell and the image fits inside it.
//
// AGENT-CONTRACT: every colour, radius, spacing and duration is a QST-1 role.
Item {
    id: tile

    required property var item
    property string previewUrl: ""
    property bool interactive: true
    property bool reducedMotion: false

    signal activated()

    readonly property bool hovered: hover.hovered && tile.interactive
    // A proportion of the tile's own height: the planner owns the cell size.
    readonly property real glyphProportion: 0.34

    objectName: "gatherWindowTile"

    Accessible.role: Accessible.Button
    Accessible.name: String(tile.item.accessibleName || tile.item.title || "")
    Accessible.description: tile.interactive
                            ? qsTr("Window. Activating it brings it forward.")
                            : qsTr("Window. Unavailable: the window list is degraded.")
    Accessible.onPressAction: if (tile.interactive) tile.activated()

    C.MaterialSurface {
        objectName: "gatherWindowTileSurface"
        anchors.fill: parent
        raised: true
        border.color: !Tokens.ready
                      ? "transparent"
                      : !tile.interactive
                        ? Tokens.fg.disabled
                        : tile.item.urgent
                          ? Tokens.danger.default
                          : tile.hovered || tile.item.active
                            ? Tokens.accent.default
                            : Tokens.accessibility.highContrast
                              ? Tokens.outline.strong
                              : Tokens.outline.divider
        // The focused window reads as focused without moving anything.
        border.width: tile.item.active && Tokens.ready ? 2 : 1

        Behavior on border.color {
            enabled: !tile.reducedMotion && Tokens.ready
            ColorAnimation { duration: Tokens.ready ? Tokens.motion.short : 0 }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Tokens.ready ? Tokens.space["2"] : 0
            spacing: Tokens.ready ? Tokens.space["1"] : 0

            Item {
                id: thumbnailArea
                objectName: "gatherWindowTileThumbnailArea"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Image {
                    id: preview
                    objectName: "gatherWindowTilePreview"
                    anchors.fill: parent
                    source: tile.previewUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                    Accessible.ignored: true
                }

                ShellIcons.Icon {
                    objectName: "gatherWindowTileIcon"
                    anchors.centerIn: parent
                    visible: preview.status !== Image.Ready
                    name: String(tile.item.iconName ?? "")
                    size: Math.max(1, Math.round(tile.height * tile.glyphProportion))
                    color: !Tokens.ready ? "transparent"
                           : tile.interactive ? Tokens.fg.default
                                              : Tokens.fg.disabled
                    fallbackText: String(tile.item.iconText
                                         || tile.item.applicationName || "")
                    Accessible.ignored: true
                }
            }

            Text {
                objectName: "gatherWindowTileTitle"
                Layout.fillWidth: true
                text: String(tile.item.title || tile.item.applicationName || "")
                elide: Text.ElideRight
                maximumLineCount: 1
                horizontalAlignment: Text.AlignHCenter
                color: !Tokens.ready ? "transparent"
                       : tile.interactive ? Tokens.fg.default : Tokens.fg.disabled
                font.family: Tokens.ready ? Tokens.type.fontFamily : ""
                font.pointSize: Tokens.ready ? Tokens.type.caption : 1
                Accessible.ignored: true
            }

            // The application behind the window, when the title does not
            // already say it. Two windows of the same program are otherwise
            // indistinguishable in a grid.
            Text {
                objectName: "gatherWindowTileApplication"
                Layout.fillWidth: true
                visible: String(tile.item.applicationName || "").length > 0
                         && String(tile.item.applicationName)
                            !== String(tile.item.title || "")
                text: String(tile.item.applicationName || "")
                elide: Text.ElideRight
                maximumLineCount: 1
                horizontalAlignment: Text.AlignHCenter
                color: Tokens.ready ? Tokens.fg.muted : "transparent"
                font.family: Tokens.ready ? Tokens.type.fontFamily : ""
                font.pointSize: Tokens.ready ? Tokens.type.caption : 1
                Accessible.ignored: true
            }
        }
    }

    HoverHandler {
        id: hover
        objectName: "gatherWindowTileHover"
        enabled: tile.interactive
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        objectName: "gatherWindowTileTap"
        enabled: tile.interactive
        onTapped: tile.activated()
    }
}

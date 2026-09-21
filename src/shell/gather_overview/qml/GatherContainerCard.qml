// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// One window container (ADR-0139) as a rolled-up card. Gather presents every
// container rolled up, so this card stands for the container whether or not it
// is really rolled up - and the member windows it holds are deliberately not
// drawn anywhere else in the overview.
//
// The card says three things: which container, how many windows are inside,
// and the colour the user gave it. It does not list members: the point of the
// card lane is that a container is ONE thing here.
//
// AGENT-CONTRACT: every colour, radius, spacing and duration is a QST-1 role.
// The single exception is `item.colorHex`, the exact "#RRGGBB" the user chose
// for this container, which is their value and not the theme's to override.
Item {
    id: card

    required property var item
    property bool interactive: true
    property bool reducedMotion: false

    signal activated()

    readonly property bool hovered: hover.hovered && card.interactive
    readonly property bool tinted: String(card.item.colorHex ?? "").length > 0
    readonly property int memberCount: Number(card.item.windowCount ?? 0)
    // A proportion of the card's own height, not a spacing token: the planner
    // owns the card size, and a spacing value is not an icon size.
    readonly property real glyphProportion: 0.5

    objectName: "gatherContainerCard"

    Accessible.role: Accessible.Button
    Accessible.name: String(card.item.accessibleName || card.item.title || "")
    Accessible.description: card.interactive
                            ? qsTr("Container of %n window(s). Activating it brings the container forward.",
                                   "", card.memberCount)
                            : qsTr("Container of %n window(s). Unavailable: the window list is degraded.",
                                   "", card.memberCount)
    Accessible.onPressAction: if (card.interactive) card.activated()

    C.MaterialSurface {
        objectName: "gatherContainerCardSurface"
        anchors.fill: parent
        raised: true
        border.color: !Tokens.ready
                      ? "transparent"
                      : !card.interactive
                        ? Tokens.fg.disabled
                        : card.hovered
                          ? Tokens.accent.default
                          : Tokens.accessibility.highContrast
                            ? Tokens.outline.strong
                            : Tokens.outline.divider

        Behavior on border.color {
            enabled: !card.reducedMotion && Tokens.ready
            ColorAnimation { duration: Tokens.ready ? Tokens.motion.short : 0 }
        }

        // The container's identity stripe down the leading edge. It wears the
        // user's own colour when they set one, and the accent role otherwise.
        Rectangle {
            objectName: "gatherContainerCardStripe"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 1
            width: Tokens.ready ? Tokens.space["1"] : 0
            radius: Tokens.ready ? Tokens.radius.s : 0
            color: !Tokens.ready ? "transparent"
                   : !card.interactive ? Tokens.fg.disabled
                   : card.tinted ? card.item.colorHex
                   : Tokens.accent.default
            Accessible.ignored: true
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Tokens.ready ? Tokens.space["3"] : 0
            anchors.rightMargin: Tokens.ready ? Tokens.space["2"] : 0
            anchors.topMargin: Tokens.ready ? Tokens.space["1"] : 0
            anchors.bottomMargin: Tokens.ready ? Tokens.space["1"] : 0
            spacing: Tokens.ready ? Tokens.space["2"] : 0

            ShellIcons.Icon {
                objectName: "gatherContainerCardIcon"
                Layout.alignment: Qt.AlignVCenter
                name: String(card.item.iconName ?? "")
                size: Math.max(1, Math.round(card.height * card.glyphProportion))
                color: !Tokens.ready ? "transparent"
                       : card.interactive ? Tokens.fg.default : Tokens.fg.disabled
                symbolic: true
                fallbackText: String(card.item.iconText || "")
                Accessible.ignored: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                Text {
                    objectName: "gatherContainerCardTitle"
                    Layout.fillWidth: true
                    text: String(card.item.title || "")
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: !Tokens.ready ? "transparent"
                           : card.interactive ? Tokens.fg.default
                                              : Tokens.fg.disabled
                    font.family: Tokens.ready ? Tokens.type.fontFamily : ""
                    font.pointSize: Tokens.ready ? Tokens.type.body : 1
                    Accessible.ignored: true
                }

                Text {
                    objectName: "gatherContainerCardSubtitle"
                    Layout.fillWidth: true
                    text: qsTr("%n window(s)", "", card.memberCount)
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: Tokens.ready ? Tokens.fg.muted : "transparent"
                    font.family: Tokens.ready ? Tokens.type.fontFamily : ""
                    font.pointSize: Tokens.ready ? Tokens.type.caption : 1
                    Accessible.ignored: true
                }
            }
        }
    }

    HoverHandler {
        id: hover
        objectName: "gatherContainerCardHover"
        enabled: card.interactive
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        objectName: "gatherContainerCardTap"
        enabled: card.interactive
        onTapped: card.activated()
    }
}

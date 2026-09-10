// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One layout-profile card: a miniature desktop rendered from the profile's
// panel summary (edge, alignment, length) plus its name. Selecting a layout
// is a visual decision — the miniature mirrors the arrangement the desktop
// will use, and the tooltip carries the catalog description.
T.AbstractButton {
    id: card

    required property var profile
    required property bool selected
    required property bool available
    readonly property int miniatureWidth: 108
    readonly property int miniatureHeight: miniatureWidth * 9 / 16
    readonly property var panelSummaries: profile.panels ?? []

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    enabled: card.available
    implicitWidth: miniatureWidth
    implicitHeight: miniatureHeight + Tokens.space["3"] + captionMetrics.height
    padding: 0

    objectName: "customizeProfileCard_" + (profile.id ?? "")
    Accessible.role: Accessible.RadioButton
    Accessible.name: qsTr("%1 layout profile").arg(profile.name ?? "")
    Accessible.description: profile.description ?? ""
    Accessible.checked: card.selected
    T.ToolTip.visible: card.hovered && (profile.description ?? "").length > 0
    T.ToolTip.delay: 500
    T.ToolTip.text: profile.description ?? ""

    onClicked: {
        if (card.customizeSettings !== null)
            card.customizeSettings.selectProfile(card.profile.id ?? "")
    }

    // The shared route model; this file avoids any other model dependency so
    // the gallery stays a pure presentation delegate.
    property var customizeSettings: null

    TextMetrics {
        id: captionMetrics
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.caption
        text: profile.name ?? ""
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            radius: Tokens.radius.m
            color: card.selected ? Tokens.accent.subtle
                 : card.hovered && card.enabled ? Tokens.state.hover : "transparent"
            border.width: card.selected ? Tokens.space["1"] : 0
            border.color: Tokens.accent.default
        }
        FocusRing {
            anchors.fill: parent
            visible: card.activeFocus
            control: card
        }
    }

    contentItem: Column {
        spacing: Tokens.space["2"]

        Rectangle {
            id: miniature

            width: card.miniatureWidth
            height: card.miniatureHeight
            radius: Tokens.radius.s
            color: Tokens.bg.highest
            border.width: Tokens.space["1"] / 2
            border.color: card.selected ? Tokens.accent.default : Tokens.outline.strong

            // Backdrop keeps the same vertical light falloff the live
            // desktop preview paints, scaled down to card size and lifted
            // above the card material so panel bars stay legible.
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: Qt.lighter(Tokens.bg.base, 1.35)
                    }
                    GradientStop {
                        position: 1.0
                        color: Qt.lighter(Tokens.bg.base, 1.1)
                    }
                }
            }

            Repeater {
                model: card.panelSummaries

                delegate: Rectangle {
                    id: bar

                    required property var modelData

                    readonly property bool horizontal:
                        modelData.edge === "top" || modelData.edge === "bottom"
                    // Card-scale panel bars: proportionally faithful edges
                    // and lengths with a legibility floor, matching how the
                    // concept preview scales real thickness.
                    readonly property real barThickness:
                        Math.max(3, Number(modelData.thickness ?? 32)
                                  * card.miniatureWidth / 1920 * 2.2)
                    readonly property real barLength:
                        card.miniatureWidth * Number(modelData.length ?? 1.0)
                    readonly property real crossLength:
                        card.miniatureHeight * Number(modelData.length ?? 1.0)

                    width: horizontal ? barLength : barThickness
                    height: horizontal ? barThickness : crossLength
                    x: modelData.edge === "left" ? 0
                       : modelData.edge === "right" ? miniature.width - width
                       : modelData.alignment === "start" ? 0
                       : modelData.alignment === "end" ? miniature.width - width
                       : (miniature.width - width) / 2
                    y: modelData.edge === "top" ? 0
                       : modelData.edge === "bottom" ? miniature.height - height
                       : modelData.alignment === "start" ? 0
                       : modelData.alignment === "end" ? miniature.height - height
                       : (miniature.height - height) / 2
                    radius: modelData.alignment === "fill" ? 0 : Tokens.radius.s
                    color: card.selected ? Tokens.accent.default : Tokens.fg.muted
                    opacity: modelData.layer === "below" ? 0.8 : 1.0
                }
            }

            // Selected marker: a small badge instead of border-only state so
            // the active layout stays identifiable while thumbnails scroll.
            Rectangle {
                visible: card.selected
                width: Tokens.space["4"]
                height: width
                radius: width / 2
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Tokens.space["1"]
                color: Tokens.accent.default
                border.width: Tokens.space["1"] / 2
                border.color: Tokens.bg.base

                Rectangle {
                    anchors.centerIn: parent
                    width: Tokens.space["1"] * 1.5
                    height: width
                    radius: width / 2
                    color: Tokens.accent.fg
                }
            }
        }

        Text {
            width: card.miniatureWidth
            text: card.profile.name ?? ""
            color: card.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
    }
}

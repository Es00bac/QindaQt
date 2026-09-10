// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// The WYSIWYG work area: a monitor whose screen renders the edited layout
// with the same design tokens the live desktop uses, so the preview tracks
// the active QindaQt theme. Panel rectangles come from the repository
// projection; only the cross-axis thickness gets a legibility floor so the
// preview stays clickable at representative scale (the desktop concept
// preview applies the same floor).
T.Control {
    id: root

    required property var customizeSettings

    padding: Tokens.space["4"]
    Accessible.ignored: true

    background: Rectangle {
        color: Tokens.bg.base
        radius: Tokens.radius.l
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
    }

    contentItem: Item {
        id: canvasHost

        // Monitor bezel keeps a 16:9 screen inside any column shape.
        Rectangle {
            id: bezel

            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height * 16 / 9)
            height: width * 9 / 16
            radius: Tokens.radius.l
            color: Tokens.bg.highest
            border.width: Tokens.space["1"]
            border.color: Tokens.outline.strong

            Item {
                id: screen

                objectName: "customizeOutputCanvas"
                anchors.fill: parent
                anchors.margins: Tokens.space["3"]

                // Themed backdrop: the same vertical light falloff the
                // desktop concept preview paints, driven entirely by the
                // live token facade. The stops stay deliberately lighter
                // than the panel material so a dark panel bar reads
                // against its wallpaper the way the real desktop does.
                Rectangle {
                    anchors.fill: parent
                    radius: Tokens.radius.s
                    clip: true
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: Qt.lighter(Tokens.bg.base, 1.32)
                        }
                        GradientStop {
                            position: 0.62
                            color: Qt.lighter(Tokens.bg.base, 1.12)
                        }
                        GradientStop {
                            position: 1.0
                            color: Tokens.bg.base
                        }
                    }

                    Rectangle {
                        width: parent.width * 0.46
                        height: width
                        radius: width / 2
                        x: parent.width * 0.38
                        y: parent.height * 0.1
                        color: Tokens.accent.default
                        opacity: 0.06
                    }
                }

                // A quiet window mock keeps the preview reading as a desktop
                // and gives floating panels a believable backdrop.
                CustomizeWindowMock {
                    width: Math.min(parent.width * 0.66, 900 * screen.width / 1920)
                    height: Math.min(parent.height * 0.52, 520 * screen.width / 1920)
                    anchors.centerIn: parent
                }

                Repeater {
                    model: root.customizeSettings.panels

                    delegate: CustomizePanelPreview {
                        required property var modelData

                        panelData: modelData
                        customizeSettings: root.customizeSettings
                        canvasScale: screen.width / 1920
                    }
                }

                Text {
                    visible: root.customizeSettings.panels.length === 0
                    anchors.centerIn: parent
                    text: qsTr("This layout has no panels yet")
                    color: Tokens.fg.muted
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.body
                }
            }
        }
    }
}

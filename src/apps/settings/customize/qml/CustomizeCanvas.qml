// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.SettingsApp.Customize 1.0
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

    // Read-only wallpaper truth published by the route model. Null (or a
    // model without the projection) keeps the token gradient; only an
    // explicit "ready" status with a resolved source paints the wallpaper.
    readonly property var wallpaperPreview: root.customizeSettings.wallpaperPreview
    readonly property bool wallpaperReady: root.wallpaperPreview !== null
                                           && root.wallpaperPreview !== undefined
                                           && root.wallpaperPreview.status === "ready"
                                           && root.wallpaperPreview.source.toString().length > 0

    // Read-only contained-window chrome truth published by the route model,
    // resolved through the same Appearance/compositor decoration pipeline a
    // real contained window paints from -- never an invented mock chrome.
    readonly property var windowPreview: root.customizeSettings.windowPreview

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
            width: parent.width < parent.height * 16 / 9
                   ? parent.width : parent.height * 16 / 9
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
                // Panel zones are descendants and therefore win hit testing;
                // all remaining screen space is the desktop applet target.
                readonly property string targetPanelId: "@desktop"
                readonly property string targetZone: "desktop"

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

                // The configured wallpaper, painted exactly the way the shell
                // paints it (scaled/centered/tiled), over the token gradient
                // that stays as the explicit fallback for no wallpaper,
                // invalid input, or unavailable Settings1 truth.
                Image {
                    objectName: "customizeCanvasWallpaper"

                    anchors.fill: parent
                    visible: root.wallpaperReady
                    source: root.wallpaperReady ? root.wallpaperPreview.source : ""
                    asynchronous: true
                    clip: true
                    fillMode: root.wallpaperReady
                              && root.wallpaperPreview.mode === "tiled" ? Image.Tile
                        : root.wallpaperReady
                          && root.wallpaperPreview.mode === "centered" ? Image.Pad
                        : Image.PreserveAspectCrop
                }

                // A truthful contained-window preview keeps the canvas
                // reading as a desktop and gives floating panels a believable
                // backdrop, painted from the same resolved chrome a real
                // contained window would show.
                CustomizeContainedWindowPreview {
                    objectName: "customizeWindowPreview"
                    width: parent.width * 0.66 < 900 * screen.width / 1920
                           ? parent.width * 0.66 : 900 * screen.width / 1920
                    height: Math.min(parent.height * 0.52, 520 * screen.width / 1920)
                    anchors.centerIn: parent
                    chrome: root.windowPreview.chrome
                }

                Flow {
                    id: desktopAppletGrid

                    objectName: "customizeDesktopAppletGrid"
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: Tokens.space["3"]
                    width: parent.width * 0.3 < 180 ? parent.width * 0.3 : 180
                    spacing: Tokens.space["2"]
                    z: 2

                    Repeater {
                        model: root.customizeSettings.desktopApplets

                        delegate: Rectangle {
                            id: desktopChip

                            required property var modelData
                            width: 64
                            height: 72
                            radius: Tokens.radius.m
                            color: selected ? Tokens.accent.subtle
                                  : hover.hovered ? Tokens.state.hover
                                  : Qt.rgba(0, 0, 0, 0.18)
                            border.width: selected || hover.hovered
                                          ? Tokens.space["1"] / 2 : 0
                            border.color: selected ? Tokens.accent.default
                                                   : Tokens.outline.strong

                            readonly property bool selected:
                                root.customizeSettings.selectedKind === "applet"
                                && root.customizeSettings.selectedAppletId
                                   === (modelData.id ?? "")
                            readonly property string dragPluginId:
                                modelData.pluginId ?? ""
                            readonly property string dragPanelId: "@desktop"
                            readonly property string dragAppletId:
                                modelData.id ?? ""

                            objectName: "customizeDesktopApplet_"
                                        + (modelData.id ?? "")
                            Accessible.role: Accessible.ListItem
                            Accessible.name: qsTr("%1 desktop applet")
                                               .arg(modelData.name ?? "")

                            Column {
                                anchors.centerIn: parent
                                spacing: Tokens.space["1"]

                                CustomizeAppletIcon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    pluginId: desktopChip.modelData.pluginId ?? ""
                                    iconSize: 32
                                }
                                Text {
                                    width: 58
                                    text: desktopChip.modelData.name ?? ""
                                    color: Tokens.fg.default
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignHCenter
                                    font.family: Tokens.type.fontFamily
                                    font.pointSize: Tokens.type.caption
                                }
                            }

                            HoverHandler { id: hover }
                            TapHandler {
                                onTapped: root.customizeSettings.selectApplet(
                                              "@desktop",
                                              desktopChip.modelData.id ?? "")
                            }
                        }
                    }
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

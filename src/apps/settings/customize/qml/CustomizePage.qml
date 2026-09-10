// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var customizeSettings
    signal closeRequested()
    signal closeCancelled()

    // The Settings Center sidebar leaves about 760px to a route in its normal
    // 960px window. Compact layouts stack the work areas behind tabs so the
    // preview keeps a usable size; wide layouts show palette and inspector
    // beside the monitor.
    readonly property bool compact: width < 1000
    property int compactSection: 0
    readonly property Item firstFocusTarget: profileGallery.count > 0
                                                  ? profileGallery.itemAtIndex(0)
                                                  : actionBar.firstFocusTarget
    readonly property bool dirty: customizeSettings.dirty

    title: qsTr("Customize")
    background: Rectangle { color: Tokens.bg.base }

    function requestClose() {
        actionBar.requestClose()
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape
                && root.customizeSettings.visualDragActive) {
            root.customizeSettings.cancelDrag()
            event.accepted = true
        } else if (event.key === Qt.Key_Delete) {
            event.accepted = root.customizeSettings.removeSelected()
        } else if (event.key === Qt.Key_Space) {
            event.accepted = root.customizeSettings.keyboardMoveMode()
        } else if ((event.modifiers & Qt.ControlModifier)
                   && (event.modifiers & Qt.ShiftModifier)
                   && event.key === Qt.Key_Left) {
            event.accepted = root.customizeSettings.keyboardStep("previous-panel")
        } else if ((event.modifiers & Qt.ControlModifier)
                   && (event.modifiers & Qt.ShiftModifier)
                   && event.key === Qt.Key_Right) {
            event.accepted = root.customizeSettings.keyboardStep("next-panel")
        } else if ((event.modifiers & Qt.AltModifier)
                   && event.key === Qt.Key_Left) {
            event.accepted = root.customizeSettings.keyboardStep("previous-zone")
        } else if ((event.modifiers & Qt.AltModifier)
                   && event.key === Qt.Key_Right) {
            event.accepted = root.customizeSettings.keyboardStep("next-zone")
        } else if ((event.modifiers & Qt.ControlModifier)
                   && event.key === Qt.Key_Left) {
            event.accepted = root.customizeSettings.keyboardStep("previous")
        } else if ((event.modifiers & Qt.ControlModifier)
                   && event.key === Qt.Key_Right) {
            event.accepted = root.customizeSettings.keyboardStep("next")
        }
    }

    Shortcut {
        sequences: [StandardKey.Undo]
        onActivated: root.customizeSettings.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        onActivated: root.customizeSettings.redo()
    }
    Shortcut {
        sequence: "Ctrl+Return"
        onActivated: root.customizeSettings.apply()
    }
    Shortcut {
        sequence: "Ctrl+Shift+Return"
        onActivated: root.customizeSettings.discard()
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: root.customizeSettings.duplicateSelected()
    }

    CustomizePointerGestures {
        anchors.fill: parent
        z: 1000
        page: root
        customizeSettings: root.customizeSettings
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["4"]
        spacing: Tokens.space["3"]

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["3"]

            Label {
                Layout.fillWidth: true
                text: qsTr("Customize")
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.title
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                objectName: "customizeStatus"
                Layout.maximumWidth: root.width / 2
                text: root.customizeSettings.statusText
                wrapMode: Text.Wrap
                color: root.customizeSettings.conflict
                       ? Tokens.fg.default : Tokens.fg.muted
                font.pointSize: Tokens.type.caption
                horizontalAlignment: Text.AlignRight
                Accessible.role: root.customizeSettings.conflict
                                 || root.customizeSettings.unavailable
                                 ? Accessible.AlertMessage : Accessible.StaticText
                Accessible.name: text
            }
        }

        Label {
            objectName: "customizeError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.customizeSettings.errorText
            wrapMode: Text.Wrap
            color: Tokens.fg.default
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        DegradedNotice {
            objectName: "customizeUnavailableNotice"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.customizeSettings.unavailable
            reason: root.customizeSettings.errorText.length > 0
                    ? root.customizeSettings.errorText
                    : qsTr("Layout editing is temporarily unavailable. Try again when Settings reconnects.")
        }

        // Layout gallery: one visual card per catalog profile. Choosing a
        // layout is a visual decision — every miniature renders the panels
        // the desktop would place, and selection updates the monitor below
        // as a draft until Apply.
        Label {
            Layout.fillWidth: true
            visible: !root.customizeSettings.unavailable
            text: qsTr("Layout profile")
            muted: true
            font.pointSize: Tokens.type.caption
            Accessible.name: text
        }

        ListView {
            id: profileGallery

            objectName: "customizeProfileSelector"
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            visible: !root.customizeSettings.unavailable
            orientation: ListView.Horizontal
            spacing: Tokens.space["2"]
            clip: true
            model: root.customizeSettings.profiles
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Layout profiles")

            delegate: CustomizeProfileCard {
                required property var modelData

                profile: modelData
                selected: modelData.id === root.customizeSettings.selectedProfileId
                available: root.customizeSettings.canEdit
                customizeSettings: root.customizeSettings
            }
        }

        RowLayout {
            objectName: "customizeWideLayout"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.compact && !root.customizeSettings.unavailable
            spacing: Tokens.space["3"]

            CustomizeAppletPalette {
                Layout.minimumWidth: 204
                Layout.preferredWidth: 212
                Layout.maximumWidth: 232
                Layout.fillHeight: true
                customizeSettings: root.customizeSettings
            }
            CustomizeCanvas {
                Layout.minimumWidth: 360
                Layout.preferredWidth: 520
                Layout.fillWidth: true
                Layout.fillHeight: true
                customizeSettings: root.customizeSettings
            }
            CustomizeProperties {
                Layout.minimumWidth: 224
                Layout.preferredWidth: 260
                Layout.maximumWidth: 288
                Layout.fillHeight: true
                customizeSettings: root.customizeSettings
            }
        }

        ColumnLayout {
            objectName: "customizeCompactLayout"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.compact && !root.customizeSettings.unavailable
            spacing: Tokens.space["2"]

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["1"]

                Repeater {
                    model: [qsTr("Arrange"), qsTr("Outline"), qsTr("Details")]

                    delegate: Button {
                        required property string modelData
                        required property int index
                        objectName: "customizeCompactTab_" + index
                        Layout.fillWidth: true
                        text: modelData
                        checkable: true
                        autoExclusive: true
                        checked: root.compactSection === index
                        emphasized: checked
                        Accessible.role: Accessible.PageTab
                        Accessible.selected: checked
                        onClicked: root.compactSection = index
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.compactSection

                ColumnLayout {
                    spacing: Tokens.space["2"]

                    CustomizeAppletPalette {
                        Layout.fillWidth: true
                        compact: true
                        customizeSettings: root.customizeSettings
                    }
                    CustomizeCanvas {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 220
                        customizeSettings: root.customizeSettings
                    }
                }

                CustomizeOutline {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    customizeSettings: root.customizeSettings
                }

                T.ScrollView {
                    clip: true
                    CustomizeProperties {
                        width: parent.width
                        customizeSettings: root.customizeSettings
                    }
                }

            }
        }

        CustomizeActionBar {
            id: actionBar
            Layout.fillWidth: true
            customizeSettings: root.customizeSettings
            onCloseRequested: root.closeRequested()
            onCloseCancelled: root.closeCancelled()
        }
    }

    Label {
        visible: false
        text: root.customizeSettings.announcement
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}

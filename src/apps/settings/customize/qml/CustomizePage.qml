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

    readonly property bool compact: width < 960
    readonly property Item firstFocusTarget: profileView.count > 0
                                                  ? profileView.itemAtIndex(0)
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
        spacing: Tokens.space["2"]

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Label {
                Layout.fillWidth: true
                text: qsTr("Customize")
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.title
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            ListView {
                id: profileView
                objectName: "customizeProfileSelector"
                Layout.preferredWidth: Math.min(420, root.width / 2)
                Layout.preferredHeight: 44
                orientation: ListView.Horizontal
                spacing: Tokens.space["1"]
                clip: true
                model: root.customizeSettings.profiles
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Layout profiles")

                delegate: Button {
                    required property var modelData
                    text: modelData.name
                    checkable: true
                    autoExclusive: true
                    checked: modelData.id === root.customizeSettings.selectedProfileId
                    emphasized: checked
                    available: root.customizeSettings.canEdit
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: checked
                    onClicked: root.customizeSettings.selectProfile(modelData.id)
                }
            }
        }

        Label {
            objectName: "customizeStatus"
            Layout.fillWidth: true
            text: root.customizeSettings.statusText
            wrapMode: Text.Wrap
            color: root.customizeSettings.conflict
                   ? Tokens.fg.default : Tokens.fg.muted
            Accessible.role: root.customizeSettings.conflict
                             || root.customizeSettings.unavailable
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
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
                    : qsTr("The profile repository, editor lease, or Settings1 transport is unavailable.")
        }

        RowLayout {
            objectName: "customizeWideLayout"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.compact && !root.customizeSettings.unavailable
            spacing: Tokens.space["2"]

            ColumnLayout {
                Layout.preferredWidth: 230
                Layout.fillHeight: true
                CustomizeAppletPalette {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 250
                    customizeSettings: root.customizeSettings
                }
                CustomizeOutline {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    customizeSettings: root.customizeSettings
                }
            }
            CustomizeCanvas {
                Layout.fillWidth: true
                Layout.fillHeight: true
                customizeSettings: root.customizeSettings
            }
            CustomizeProperties {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                customizeSettings: root.customizeSettings
            }
        }

        T.ScrollView {
            objectName: "customizeCompactLayout"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.compact && !root.customizeSettings.unavailable
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: Tokens.space["2"]
                CustomizeAppletPalette {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 140
                    compact: true
                    customizeSettings: root.customizeSettings
                }
                CustomizeCanvas {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(300, width * 9 / 16)
                    customizeSettings: root.customizeSettings
                }
                CustomizeOutline {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 320
                    customizeSettings: root.customizeSettings
                }
                CustomizeProperties {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 420
                    customizeSettings: root.customizeSettings
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

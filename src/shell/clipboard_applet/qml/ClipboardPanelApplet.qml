// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

Item {
    id: root

    required property var controller
    required property var theme
    property bool vertical: false
    readonly property bool available: controller !== null
    readonly property string phase: available
        ? String(controller.phaseText ?? "unavailable") : "unavailable"

    objectName: "clipboardPanelApplet"
    implicitWidth: 32
    implicitHeight: 28

    ToolButton {
        id: summary

        objectName: "clipboardPanelSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        checkable: false
        checked: historyPopup.opened
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Clipboard history")
        Accessible.description: root.available
            ? qsTr("Open clipboard history; current state is %1").arg(root.phase)
            : qsTr("Clipboard history is unavailable")
        Accessible.checkable: false

        function togglePopup() {
            if (!root.available)
                return
            if (historyPopup.opened)
                historyPopup.close()
            else
                historyPopup.open()
        }

        onClicked: togglePopup()
        Accessible.onPressAction: togglePopup()
        Keys.onReturnPressed: event => {
            togglePopup()
            event.accepted = true
        }
        Keys.onEnterPressed: event => {
            togglePopup()
            event.accepted = true
        }

        contentItem: ShellIcons.Icon {
            objectName: "clipboardPanelIcon"
            anchors.centerIn: parent
            name: "edit-paste"
            size: Math.min(20, root.height - Tokens.space["3"])
            color: Tokens.fg.default
            symbolic: true
            fallbackText: qsTr("Clipboard")
            Accessible.ignored: true
        }
        background: Item {}
    }

    Popup {
        id: historyPopup

        objectName: "clipboardPanelPopup"
        popupType: Popup.Window
        modal: false
        focus: true
        padding: 12
        width: 404
        height: Math.min(560, Math.max(160, popupContent.implicitHeight + 24))
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                     | Popup.CloseOnPressOutsideParent

        // AGENT-GUARD: RuntimePanel rejects focus by design. This independent
        // popup window is the keyboard-capable surface; changing it back to an
        // item popup strands Tab and Escape on the layer-shell dock.
        onOpened: Qt.callLater(function() { clipboardSurface.focusInitial() })
        onClosed: summary.forceActiveFocus(Qt.PopupFocusReason)

        background: Rectangle {
            readonly property var colors: root.theme.colors ?? ({})
            radius: root.theme.cornerRadius ?? 10
            color: colors.surface ?? "#222624"
            border.color: colors.border ?? "#3c433f"
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: popupContent

            spacing: 8

            ClipboardApplet {
                id: clipboardSurface

                Layout.fillWidth: true
                controller: root.controller
            }

            C.Button {
                id: closeButton

                objectName: "clipboardPanelCloseButton"
                Layout.alignment: Qt.AlignRight
                emphasized: false
                text: qsTr("Close")
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close clipboard history")
                onClicked: historyPopup.close()
            }
        }
    }
}

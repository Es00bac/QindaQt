// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as QQ
import QindaQt.Tokens 1.0

T.Popup {
    id: root
    objectName: "formatMediaDialog"
    required property var controller
    readonly property var target: controller.formatTarget
    property string capturedToken: ""
    modal: true
    focus: true
    padding: Tokens.space["6"]
    closePolicy: T.Popup.CloseOnEscape
    background: Rectangle {
        color: Tokens.bg.raised
        radius: Tokens.radius.l
        border.color: Tokens.outline.strong
        border.width: 1
    }

    function syncTarget() {
        const token = root.target.token || ""
        if (token.length === 0) {
            root.close()
            return
        }
        if (token !== capturedToken) {
            capturedToken = token
            typedDevice.clear()
            newLabel.clear()
            filesystem.currentIndex = 0
        }
        if (!root.opened) root.open()
    }
    Connections {
        target: root.controller
        function onChanged() { root.syncTarget() }
    }
    onOpened: typedDevice.forceActiveFocus()
    onClosed: {
        typedDevice.clear()
        newLabel.clear()
        capturedToken = ""
        controller.cancelFormat()
    }

    contentItem: ColumnLayout {
        spacing: Tokens.space["4"]
        QQ.SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Erase and format this media?")
            description: qsTr("All files on %1 (%2) will be erased. This cannot be undone.")
                .arg(root.target.label || qsTr("this media")).arg(root.target.device || "")
        }
        QQ.Label {
            Layout.fillWidth: true
            text: qsTr("Target: %1 · %2").arg(root.target.device || "").arg(root.target.sizeText || "")
            wrapMode: Text.WrapAnywhere
        }
        QQ.Label {
            Layout.fillWidth: true
            visible: !!root.target.mounted
            text: qsTr("Unmount this media before formatting it.")
            wrapMode: Text.Wrap
        }
        QQ.Button {
            objectName: "unmountFormatTargetButton"
            text: qsTr("Unmount this media")
            emphasized: false
            visible: !!root.target.mounted
            available: !root.controller.busy
            onClicked: root.controller.unmount(root.capturedToken)
        }
        QQ.Label { text: qsTr("File system") }
        QQ.ComboBox {
            id: filesystem
            objectName: "formatFilesystem"
            Layout.fillWidth: true
            model: root.controller.formatTypes.map(function(type) {
                const names = { vfat: qsTr("FAT"), exfat: qsTr("exFAT"), ext4: qsTr("Linux (ext4)") }
                return { filesystem: type, name: names[type] || type }
            })
            textRole: "name"
            valueRole: "filesystem"
            Accessible.name: qsTr("New file system")
            enabled: !root.controller.busy
        }
        QQ.TextField {
            id: newLabel
            objectName: "formatLabel"
            Layout.fillWidth: true
            placeholderText: qsTr("New label (optional)")
            accessibleName: qsTr("New media label")
            enabled: !root.controller.busy
        }
        QQ.Label {
            Layout.fillWidth: true
            text: qsTr("Type %1 to confirm erasing it.").arg(root.target.device || "")
            wrapMode: Text.WrapAnywhere
        }
        QQ.TextField {
            id: typedDevice
            objectName: "formatTypedDevice"
            Layout.fillWidth: true
            accessibleName: qsTr("Device name to confirm erasing")
            enabled: !root.controller.busy
        }
        RowLayout {
            Layout.fillWidth: true
            QQ.Button {
                objectName: "cancelFormatButton"
                text: qsTr("Cancel")
                emphasized: false
                available: !root.controller.busy
                onClicked: root.close()
            }
            Item { Layout.fillWidth: true }
            QQ.Button {
                objectName: "confirmFormatButton"
                text: qsTr("Erase and format")
                destructive: true
                available: !root.controller.busy && !root.target.mounted
                    && !!root.target.canFormat && !root.target.readOnly
                    && (filesystem.currentValue || "").length > 0
                    && (root.target.device || "").length > 0
                    && typedDevice.text === root.target.device
                onClicked: {
                    root.controller.confirmFormat(filesystem.currentValue, newLabel.text, typedDevice.text)
                    typedDevice.clear()
                }
            }
        }
    }
}

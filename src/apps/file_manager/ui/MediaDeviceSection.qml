// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property var presenter: null
    spacing: 4
    visible: presenter !== null

    Label {
        text: qsTr("Devices")
        color: palette.placeholderText
        Accessible.ignored: true
    }
    Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: root.presenter !== null && root.presenter.state !== "ready"
        text: root.presenter !== null && root.presenter.state === "loading"
            ? qsTr("Loading devices…") : qsTr("Device information is unavailable.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    Button {
        objectName: "mediaRecovery"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: root.presenter !== null && root.presenter.state !== "ready"
        enabled: root.presenter !== null && root.presenter.state !== "loading"
        text: root.presenter ? root.presenter.recoveryLabel : ""
        Accessible.description: qsTr("Retry observation or deliberately start the graphical Removable Media app")
        contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
        onClicked: root.presenter.recover()
    }
    ToolButton {
        objectName: "mediaOwnerDetails"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: root.presenter !== null && root.presenter.state !== "ready"
        text: qsTr("Open Removable Media")
        contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
        onClicked: root.presenter.openOwner()
    }
    Repeater {
        model: root.presenter ? root.presenter.rows : []
        ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
        Layout.minimumWidth: 0
            spacing: 2
            Button {
                objectName: "mediaOpen_" + modelData.handle
                Layout.fillWidth: true
        Layout.minimumWidth: 0
                enabled: modelData.openEnabled
                text: modelData.name
                Accessible.name: qsTr("Open device %1").arg(modelData.name)
                Accessible.description: modelData.status + ". " + modelData.openReason
                contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                ToolTip.text: modelData.openReason
                ToolTip.visible: hovered && ToolTip.text.length > 0
                onClicked: root.presenter.open(modelData.handle)
            }
            Label {
                Layout.fillWidth: true
        Layout.minimumWidth: 0
                text: modelData.kind + " · " + modelData.status
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            // Two short rows also fit the compact 148px window sidebar.
            RowLayout {
                Layout.fillWidth: true
        Layout.minimumWidth: 0
                ToolButton {
                    objectName: "mediaReadOnly_" + modelData.handle
                    Layout.fillWidth: true
        Layout.minimumWidth: 0
                    text: qsTr("Read-only")
                    enabled: modelData.readOnlyEnabled
                    Accessible.name: qsTr("Mount %1 read-only").arg(modelData.name)
                    Accessible.description: modelData.readOnlyReason
                    ToolTip.text: modelData.readOnlyReason
                    ToolTip.visible: hovered && ToolTip.text.length > 0
                    contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                    onClicked: root.presenter.mountReadOnly(modelData.handle)
                }
                ToolButton {
                    objectName: "mediaUnmount_" + modelData.handle
                    Layout.fillWidth: true
        Layout.minimumWidth: 0
                    text: qsTr("Unmount")
                    enabled: modelData.unmountEnabled
                    Accessible.name: qsTr("Unmount %1").arg(modelData.name)
                    Accessible.description: modelData.unmountReason
                    ToolTip.text: modelData.unmountReason
                    ToolTip.visible: hovered && ToolTip.text.length > 0
                    contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                    onClicked: root.presenter.unmount(modelData.handle)
                }
            }
            RowLayout {
                Layout.fillWidth: true
        Layout.minimumWidth: 0
                ToolButton {
                    objectName: "mediaRemove_" + modelData.handle
                    Layout.fillWidth: true
        Layout.minimumWidth: 0
                    text: qsTr("Eject")
                    enabled: modelData.removeEnabled
                    Accessible.name: qsTr("Safely remove %1").arg(modelData.name)
                    Accessible.description: modelData.removeReason
                    ToolTip.text: modelData.removeReason
                    ToolTip.visible: hovered && ToolTip.text.length > 0
                    contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                    onClicked: root.presenter.remove(modelData.handle)
                }
                ToolButton {
                    objectName: "mediaDetails_" + modelData.handle
                    Layout.fillWidth: true
        Layout.minimumWidth: 0
                    text: qsTr("Details")
                    enabled: modelData.detailsEnabled
                    Accessible.name: qsTr("Details for %1").arg(modelData.name)
                    Accessible.description: modelData.detailsReason
                    ToolTip.text: modelData.detailsReason
                    ToolTip.visible: hovered && ToolTip.text.length > 0
                    contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                    onClicked: root.presenter.details(modelData.handle)
                }
            }
        }
    }
    Label {
        objectName: "mediaNotice"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: text.length > 0
        text: root.presenter ? root.presenter.notice : ""
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        Accessible.name: text
    }
    ToolButton {
        objectName: "mediaRefresh"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: root.presenter !== null && root.presenter.state === "ready"
        text: qsTr("Refresh devices")
        enabled: root.presenter !== null && !root.presenter.busy
        onClicked: root.presenter.refresh()
    }
}

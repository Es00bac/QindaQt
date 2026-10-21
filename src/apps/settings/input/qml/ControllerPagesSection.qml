// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root
    required property var controllerModel
    property bool editing: false
    property bool creating: false
    property string draftController: ""
    property string draftPage: ""
    property string draftName: ""
    property string draftColor: ""
    readonly property var page: controllerModel.currentPage
    signal captureActivityChanged(bool active)
    spacing: Tokens.space["2"]
    function edit(create) {
        root.creating = create
        root.draftController = controllerModel.selectedId
        root.draftPage = root.page.id
        root.draftName = create ? qsTr("Profile %1").arg(controllerModel.pages.length + 1) : root.page.name
        const colors = ["#287DFF", "#A83DFF", "#24D16B", "#FF8A24", "#FF304D"]
        root.draftColor = create ? colors[controllerModel.pages.length % colors.length] : root.page.color
        root.editing = true
    }
    SectionHeader { Layout.fillWidth: true; title: qsTr("Button profiles") }
    Label {
        Layout.fillWidth: true
        text: qsTr("Each profile is a page of button bindings with its own LED color. Mouse and gyro preferences stay shared. Choose a page here to activate and edit it.")
        wrapMode: Text.Wrap
    }
    FormRow {
        Layout.fillWidth: true
        label: qsTr("Active profile")
        editor: RowLayout {
            Rectangle { width: 20; height: 20; radius: 10; color: root.page.color ?? "transparent"; border.width: 1; border.color: Tokens.outline.strong }
            ComboBox {
                objectName: "controllerActiveProfile"
                Layout.preferredWidth: 280
                model: root.controllerModel.pages
                textRole: "name"; valueRole: "id"
                currentIndex: Math.max(0, root.controllerModel.pages.findIndex(page => page.id === root.page.id))
                enabled: root.controllerModel.available && !root.controllerModel.busy
                Accessible.name: qsTr("Active button profile")
                onActivated: index => {
                    root.controllerModel.setOption("activePage", root.controllerModel.pages[index].id)
                    currentIndex = Qt.binding(() => Math.max(0, root.controllerModel.pages.findIndex(page => page.id === root.page.id)))
                }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { objectName: "controllerNewProfile"; text: qsTr("New profile"); enabled: !root.controllerModel.busy && root.controllerModel.pages.length < 12; onClicked: root.edit(true) }
        Button { text: qsTr("Name and color…"); enabled: !root.controllerModel.busy; onClicked: root.edit(false) }
        Button {
            text: qsTr("Delete profile…")
            enabled: !root.controllerModel.busy && root.controllerModel.pages.length > 1
            onClicked: { root.draftController = root.controllerModel.selectedId; root.draftPage = root.page.id; remove.open() }
        }
    }
    FormRow {
        Layout.fillWidth: true
        label: qsTr("Cycle profiles")
        description: qsTr("Assign Next button profile to this button on every page")
        editor: ComboBox {
            objectName: "controllerCycleProfileButton"
            width: 320
            readonly property var choices: [{id:"", label:qsTr("Choose a button…")}].concat(root.controllerModel.buttons)
            model: choices
            textRole: "label"; valueRole: "id"
            currentIndex: Math.max(0, choices.findIndex(button => button.id === root.controllerModel.pageSwitcher))
            enabled: !root.controllerModel.busy
            Accessible.name: qsTr("Button to cycle every profile")
            onActivated: index => {
                if (choices[index].id.length > 0) root.controllerModel.setPageSwitcher(choices[index].id)
                currentIndex = Qt.binding(() => Math.max(0, choices.findIndex(button => button.id === root.controllerModel.pageSwitcher)))
            }
        }
    }
    Label {
        Layout.fillWidth: true
        visible: root.controllerModel.selected.ledStatus === "failed" || root.controllerModel.selected.ledStatus === "unsupported"
        text: root.controllerModel.selected.ledStatus === "failed" ? qsTr("The controller did not accept the LED color. Its profile bindings are still active.") : qsTr("This controller has no RGB LED. Profile switching is available.")
        wrapMode: Text.Wrap
        muted: true
    }
    Loader {
        active: root.editing
        sourceComponent: ControllerPageEditor {
            controllerModel: root.controllerModel
            creating: root.creating
            controllerId: root.draftController
            pageId: root.draftPage
            initialLayer:root.creating ? false : Boolean(root.page.layer)
            initialName: root.draftName
            initialColor: root.draftColor
            Component.onCompleted: { open(); root.captureActivityChanged(true) }
            onClosed: { root.captureActivityChanged(false); root.editing = false }
        }
    }
    T.Dialog {
        id: remove
        parent: T.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent ? parent.width - 32 : 440)
        modal: true
        focus: true
        title: qsTr("Delete button profile?")
        standardButtons: T.Dialog.Cancel | T.Dialog.Ok
        contentItem: Label { text: qsTr("This removes the active page and its bindings. Buttons that switch directly to it will be cleared."); wrapMode: Text.Wrap }
        onOpened: root.captureActivityChanged(true)
        onClosed: root.captureActivityChanged(false)
        onAccepted: if (root.draftController === root.controllerModel.selectedId && root.draftPage === root.page.id) root.controllerModel.removePage()
    }
}

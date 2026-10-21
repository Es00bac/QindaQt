// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaQt.Controls 1.0

FormRow {
    id: root
    required property var binding
    required property var controllerModel
    property var macroModel:null
    property bool editing: false
    property var draft: ({})
    property string draftController: ""
    property string draftPage: ""
    signal captureActivityChanged(bool active)
    label: binding.label
    description: binding.action === "dictate" ? qsTr("Hold while speaking; release to insert the text")
               : binding.action === "voice-command" ? qsTr("Hold while speaking; release to run the command") : ""
    editor: Button {
        objectName: "controllerBinding_" + root.binding.id
        text: root.binding.summary ?? qsTr("Choose a binding")
        width: 320
        enabled: root.controllerModel.available && !root.controllerModel.busy
        Accessible.name: root.binding.label + ": " + text
        onClicked: {
            root.draft = JSON.parse(JSON.stringify(root.binding))
            root.draftController = root.controllerModel.selectedId
            root.draftPage = root.controllerModel.currentPage.id
            root.editing = true
        }
    }
    // Construct popups only while used: a controller can expose 28 buttons.
    Loader {
        active: root.editing
        sourceComponent: ControllerBindingEditor {
            binding: root.draft
            controllerModel: root.controllerModel
            macroModel: root.macroModel
            controllerId: root.draftController
            pageId: root.draftPage
            Component.onCompleted: { open(); root.captureActivityChanged(true) }
            onClosed: { root.captureActivityChanged(false); root.editing = false }
        }
    }
}

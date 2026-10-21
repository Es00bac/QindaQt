// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Dialog {
    id: root
    required property var controllerModel
    required property bool creating
    required property string controllerId
    required property string pageId
    required property string initialName
    required property string initialColor
    property bool initialLayer:false
    property string colorDraft: initialColor
    property string failure: ""
    readonly property bool validColor: /^#[0-9a-fA-F]{6}$/.test(colorDraft)
    readonly property color preview: validColor ? colorDraft : "transparent"
    readonly property bool stale: controllerId !== controllerModel.selectedId || pageId !== (controllerModel.currentPage.id ?? "") || !controllerModel.available
    parent: T.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(580, parent ? parent.width - 32 : 580)
    modal: true
    focus: true
    title: creating ? qsTr("New button profile") : qsTr("Edit button profile")
    onOpened: name.forceActiveFocus()
    contentItem: ColumnLayout {
        spacing: Tokens.space["3"]
        TextField {
            id: name
            objectName: "controllerProfileName"
            Layout.fillWidth: true
            text: root.initialName
            maximumLength: 48
            accessibleName: qsTr("Profile name")
            placeholderText: qsTr("Profile name")
        }
        Label { text: qsTr("Controller LED color"); font.weight: Font.DemiBold }
        RowLayout {
            Layout.fillWidth: true
            Rectangle { width: 32; height: 32; radius: 16; color: root.preview; border.width: 1; border.color: Tokens.outline.strong; Accessible.name: qsTr("LED preview %1").arg(root.colorDraft) }
            TextField {
                id: color
                objectName: "controllerProfileColor"
                Layout.fillWidth: true
                text: root.colorDraft
                maximumLength: 7
                accessibleName: qsTr("LED color in hex")
                placeholderText: "#RRGGBB"
                onTextEdited: root.colorDraft = text
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]
            // These are editable controller RGB values, not interface tokens.
            Repeater {
                model: [{name:qsTr("Blue"), color:"#287DFF"}, {name:qsTr("Purple"), color:"#A83DFF"}, {name:qsTr("Green"), color:"#24D16B"}, {name:qsTr("Orange"), color:"#FF8A24"}, {name:qsTr("Red"), color:"#FF304D"}, {name:qsTr("Off"), color:"#000000"}]
                delegate: Button {
                    required property var modelData
                    text: modelData.name
                    onClicked: { root.colorDraft = modelData.color; color.text = Qt.binding(() => root.colorDraft) }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("The LED follows the active profile when desktop control is active. Any RGB hex color works. Devices without a color LED still use every profile.")
            wrapMode: Text.Wrap
            muted: true
        }
        CheckBox { id:layer; text:qsTr("Temporary layer — skip when cycling profiles"); checked:root.initialLayer }
        CheckBox { id: copy; visible: root.creating; text: qsTr("Copy the current button bindings"); checked: true }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.stale ? qsTr("The selected controller or profile changed. Reopen this editor.") : root.failure
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Cancel"); onClicked: root.close() }
            Button {
                objectName: "controllerProfileSave"
                text: root.creating ? qsTr("Create profile") : qsTr("Save profile")
                emphasized: true
                enabled: !root.stale && !root.controllerModel.busy && name.text.trim().length > 0 && root.validColor
                onClicked: {
                    const ok = root.creating ? root.controllerModel.addPage(name.text, root.colorDraft, copy.checked, layer.checked) : root.controllerModel.updatePage(name.text, root.colorDraft, layer.checked)
                    if (ok) root.close()
                    else root.failure = qsTr("The profile could not be saved. Check its name and color.")
                }
            }
        }
    }
}

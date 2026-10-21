// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

InputEditorDialog {
    id: root
    required property var binding
    required property var controllerModel
    required property string controllerId
    required property string pageId
    property var macroModel:null
    property bool macroEditing:false
    property var macroDraft:({})
    property string macroId:binding.macro ?? ""
    property string action: binding.action === "shortcut" ? "combination" : binding.action
    property bool awaitingSave:false
    property string failure: ""
    readonly property bool stale: controllerId !== controllerModel.selectedId || pageId !== (controllerModel.currentPage.id ?? "") || !controllerModel.available
    readonly property var editableActions: controllerModel.actions.filter(item => item.value !== "shortcut")
    readonly property var value: action === "combination" ? combination.value
        : action === "macro" ? ({action:"macro", macro:macroId})
        : action === "text" ? ({action: action, text: textBlock.text})
        : (action === "switch-page" || action === "hold-page") ? ({action: action, page: target.currentValue ?? ""}) : ({action: action})
    readonly property string validation: action === "macro" && (!macroModel || !macroModel.available || !macroModel.macros.some(macro => macro.id === macroId)) ? qsTr("Choose a saved macro or create one.") : controllerModel.bindingError(value)
    parent: T.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(680, parent ? parent.width - 32 : 680)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : 760)
    modal: true
    focus: true
    title: qsTr("Bind %1").arg(binding.label)
    closePolicy: T.Popup.CloseOnEscape
    onOpened: actionChoice.forceActiveFocus()
    Connections {
        target:root.controllerModel
        function onEditCompleted(ok) {
            if (!root.awaitingSave) return
            root.awaitingSave = false
            if (ok) root.close(); else root.failure = root.controllerModel.errorText || qsTr("The binding was not saved. Your draft is still here.")
        }
    }
    contentItem: T.ScrollView {
        clip: true
        implicitHeight: body.implicitHeight
        contentWidth: availableWidth
        ColumnLayout {
            id: body
            width: parent.width
            spacing: Tokens.space["3"]
            ComboBox {
                id: actionChoice
                objectName: "controllerBindingType"
                Layout.fillWidth: true
                model: root.editableActions
                textRole: "label"; valueRole: "value"
                currentIndex: Math.max(0, root.editableActions.findIndex(item => item.value === root.action))
                Accessible.name: qsTr("Binding type")
                onActivated: index => { root.action = root.editableActions[index].value; currentIndex = Qt.binding(() => Math.max(0, root.editableActions.findIndex(item => item.value === root.action))) }
            }
            InputCombinationEditor {
                id: combination
                Layout.fillWidth: true
                visible: root.action === "combination"
                bindingModel: root.controllerModel
                initialBinding: root.controllerModel.combination(root.binding)
            }
            T.TextArea {
                id: textBlock
                objectName: "controllerTextBlock"
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                visible: root.action === "text"
                text: root.binding.text ?? ""
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                placeholderText: qsTr("Enter the text to insert, including line breaks…")
                color: Tokens.fg.default
                selectionColor: Tokens.accent.default
                selectedTextColor: Tokens.accent.fg
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                padding: Tokens.space["3"]
                Accessible.name: qsTr("Text block")
                background: Rectangle { color: Tokens.bg.raised; radius: Tokens.radius.s; border.color: textBlock.activeFocus ? Tokens.focus.ring : Tokens.outline.strong; border.width: 1 }
            }
            ComboBox {
                id: target
                objectName: "controllerTargetProfile"
                Layout.fillWidth: true
                visible: root.action === "switch-page" || root.action === "hold-page"
                model: root.controllerModel.pages
                textRole: "name"; valueRole: "id"
                currentIndex: Math.max(0, root.controllerModel.pages.findIndex(page => page.id === root.binding.page))
                Accessible.name: qsTr("Target button profile")
            }
            ComboBox {
                visible:root.action === "macro"
                Layout.fillWidth:true
                model:root.macroModel ? root.macroModel.macros : []
                textRole:"name"; valueRole:"id"
                currentIndex:root.macroModel ? root.macroModel.macros.findIndex(macro => macro.id === root.macroId) : -1
                Accessible.name:qsTr("Macro to run")
                onActivated:index => root.macroId = root.macroModel.macros[index].id
            }
            Button { emphasized: false;
                visible:root.action === "macro"
                text:qsTr("Create a macro…")
                enabled:root.macroModel !== null && root.macroModel.available && !root.macroModel.busy
                onClicked:{ root.macroDraft = root.macroModel.create(); root.macroEditing = true }
            }
            Label {
                Layout.fillWidth: true
                visible: root.action === "text" || root.action.endsWith("page") || root.action === "dictate" || root.action === "voice-command"
                text: root.action === "text" ? qsTr("Inserts once per press, preserving Unicode and line breaks. Up to 4,096 UTF-8 bytes. Switching windows or pausing controller input cancels the remaining text.")
                    : root.action === "dictate" || root.action === "voice-command" ? qsTr("Hold while speaking; release to finish.")
                    : qsTr("Changes the button map and controller LED color once per press. Assign Next button profile to the same button on each page to cycle back.")
                wrapMode: Text.Wrap
                muted: true
            }
            Label {
                objectName: "controllerBindingValidation"
                Layout.fillWidth: true
                text: root.stale ? qsTr("The selected controller or profile changed. Close and reopen this editor.") : root.failure.length > 0 ? root.failure : root.validation
                visible: text.length > 0
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
            }

        }
    }
    Loader {
        active:root.macroEditing
        sourceComponent:InputMacroEditor {
            macroModel:root.macroModel; initialMacro:root.macroDraft; expected:""
            Component.onCompleted:open()
            onSaved:id => root.macroId = id
            onClosed:root.macroEditing = false
        }
    }
    footer: InputEditorFooter {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button { emphasized: false; text: qsTr("Cancel"); onClicked: root.close() }
        Button {
            objectName: "controllerBindingSave"
            text: qsTr("Save binding")
            emphasized: true
            enabled: !root.stale && !root.controllerModel.busy && root.validation.length === 0
            onClicked: {
                root.awaitingSave = true
                if (!root.controllerModel.saveBinding(root.binding.id, root.value, root.controllerId, root.pageId, root.binding)) {
                    root.awaitingSave = false
                    root.failure = qsTr("This binding could not be saved. Check the controller and profile, then try again.")
                }
            }
        }
    }
}

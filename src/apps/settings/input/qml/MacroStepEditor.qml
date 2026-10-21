// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Dialog {
    id: root
    required property var macroModel
    required property var initialStep
    property string kind: initialStep.kind ?? "tap"
    property string action: initialStep.binding?.action === "shortcut" ? "combination" : (initialStep.binding?.action ?? "combination")
    readonly property var stepTypes: [{value:"tap", label:qsTr("Tap keys / click / action")}, {value:"press", label:qsTr("Begin a hold")}, {value:"release", label:qsTr("Release a hold")}, {value:"text", label:qsTr("Insert text")}, {value:"wait", label:qsTr("Wait")}, {value:"move", label:qsTr("Move pointer")}, {value:"scroll", label:qsTr("Scroll")}]
    readonly property var choices: macroModel.actions.filter(item => kind !== "press" || ["combination","left-click","right-click","middle-click"].includes(item.value))
    readonly property var value: buildValue()
    signal saved(var step)
    function buildValue() {
        const step = {kind:root.kind}
        if (kind === "tap" || kind === "press") step.binding = root.action === "combination" ? combination.value : {action:root.action}
        if (kind === "text") step.binding = {action:"text", text:textBlock.text}
        if (kind === "press" || kind === "release") step.slot = slot.currentIndex
        if (kind === "tap" || kind === "wait") step.milliseconds = Number(duration.text)
        if (kind === "move" || kind === "scroll") { step.x = Number(dx.text); step.y = Number(dy.text) }
        return step
    }
    parent: T.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(660, parent ? parent.width - 32 : 660)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : 760)
    modal: true; focus: true
    title: qsTr("Macro step")
    contentItem: T.ScrollView {
        clip: true
        implicitHeight: body.implicitHeight
        contentWidth: availableWidth
        ColumnLayout {
            id: body
            width: parent.width
            spacing: Tokens.space["2"]
            ComboBox {
                Layout.fillWidth: true
                model: root.stepTypes; textRole:"label"; valueRole:"value"
                currentIndex: Math.max(0, root.stepTypes.findIndex(step => step.value === root.kind))
                Accessible.name: qsTr("Step type")
                onActivated: index => { root.kind = root.stepTypes[index].value; if (root.kind === "press" && !["combination","left-click","right-click","middle-click"].includes(root.action)) root.action = "combination" }
            }
            ComboBox {
                visible: root.kind === "tap" || root.kind === "press"
                Layout.fillWidth: true
                model: root.choices; textRole:"label"; valueRole:"value"
                currentIndex: Math.max(0, root.choices.findIndex(item => item.value === root.action))
                Accessible.name: qsTr("Step action")
                onActivated: index => root.action = root.choices[index].value
            }
            InputCombinationEditor {
                id: combination
                Layout.fillWidth: true
                visible: (root.kind === "tap" || root.kind === "press") && root.action === "combination"
                bindingModel: root.macroModel
                initialBinding: root.macroModel.combination(root.initialStep.binding ?? {action:"combination", modifiers:[], keys:["K"], mouseButtons:[]})
                showLayer: false
            }
            FormRow {
                Layout.fillWidth: true
                visible: root.kind === "press" || root.kind === "release"
                label: qsTr("Hold number")
                description: qsTr("Begin and release the same number to hold keys or buttons across steps")
                editor: ComboBox { id:slot; model:[1,2,3,4,5,6,7,8]; currentIndex:root.initialStep.slot ?? 0; Accessible.name:qsTr("Hold number") }
            }
            FormRow {
                Layout.fillWidth: true
                visible: root.kind === "tap" || root.kind === "wait"
                label: root.kind === "tap" ? qsTr("Hold for (ms)") : qsTr("Wait (ms)")
                editor: TextField { id:duration; width:160; text:String(root.initialStep.milliseconds ?? 40); accessibleName:qsTr("Duration in milliseconds"); inputMethodHints:Qt.ImhDigitsOnly }
            }
            RowLayout {
                visible: root.kind === "move" || root.kind === "scroll"
                Layout.fillWidth: true
                Label { text:qsTr("Horizontal") }
                TextField { id:dx; Layout.fillWidth:true; text:String(root.initialStep.x ?? 0); accessibleName:qsTr("Horizontal movement") }
                Label { text:qsTr("Vertical") }
                TextField { id:dy; Layout.fillWidth:true; text:String(root.initialStep.y ?? 0); accessibleName:qsTr("Vertical movement") }
            }
            T.TextArea {
                id:textBlock
                objectName:"macroStepText"
                Layout.fillWidth:true; Layout.preferredHeight:200
                visible:root.kind === "text"
                text:root.initialStep.binding?.text ?? ""
                textFormat:TextEdit.PlainText; wrapMode:TextEdit.Wrap; selectByMouse:true
                color:Tokens.fg.default; selectionColor:Tokens.accent.default; selectedTextColor:Tokens.accent.fg
                font.family:Tokens.type.fontFamily; font.pointSize:Tokens.type.body
                padding:Tokens.space["3"]
                Accessible.name:qsTr("Step text")
                background:Rectangle { color:Tokens.bg.raised; border.color:Tokens.outline.strong; border.width:1; radius:Tokens.radius.s }
            }
            Label { Layout.fillWidth:true; text:root.macroModel.stepError(root.value); visible:text.length > 0; wrapMode:Text.Wrap; Accessible.role:Accessible.AlertMessage }
            RowLayout {
                Layout.fillWidth:true
                Item { Layout.fillWidth:true }
                Button { text:qsTr("Cancel"); onClicked:root.close() }
                Button { objectName:"macroStepSave"; text:qsTr("Use step"); emphasized:true; enabled:root.macroModel.stepError(root.value).length === 0; onClicked:{ root.saved(root.value); root.close() } }
            }
        }
    }
}

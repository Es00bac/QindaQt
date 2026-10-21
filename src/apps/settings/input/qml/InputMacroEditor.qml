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
    required property var initialMacro
    required property string expected
    property var steps: JSON.parse(JSON.stringify(initialMacro.steps ?? []))
    property string failure:""
    property bool authorityLost:false
    property bool stepEditing:false
    property int stepIndex:-1
    property var stepDraft:({})
    property var mouseModifiers:Array.from(initialMacro.mouseModifiers ?? [])
    readonly property var value: ({id:initialMacro.id, name:name.text, shortcut:shortcut.sequence ? macroModel.portableKey(shortcut.sequence) : "", mouseButton:mouse.currentValue ?? "", mouseModifiers:(mouse.currentValue ?? "").length > 0 ? mouseModifiers : [], steps:steps})
    readonly property bool stale: authorityLost || macroModel.fingerprint(initialMacro.id) !== expected
    signal saved(string id)
    function editStep(index) {
        root.stepIndex = index
        root.stepDraft = index < 0 ? {kind:"tap", milliseconds:40, binding:{action:"combination", modifiers:[], keys:["K"], mouseButtons:[]}} : JSON.parse(JSON.stringify(root.steps[index]))
        root.stepEditing = true
    }
    function updateStep(step) {
        const next = root.steps.slice()
        if (root.stepIndex < 0) next.push(step); else next[root.stepIndex] = step
        root.steps = next
    }
    function moveStep(index, direction) {
        const next = root.steps.slice(); const target = index + direction
        if (target < 0 || target >= next.length) return
        const value = next[index]; next[index] = next[target]; next[target] = value; root.steps = next
    }
    function addRecorded(binding) {
        if (root.steps.length >= 64) return
        root.steps = root.steps.concat([{kind:"tap", milliseconds:40, binding:binding}])
    }
    parent:T.Overlay.overlay
    anchors.centerIn:parent
    width:Math.min(820, parent ? parent.width - 32 : 820)
    height:Math.min(implicitHeight, parent ? parent.height - 32 : 820)
    modal:true; focus:true
    title:qsTr("Macro editor")
    closePolicy: recorder.recording ? T.Popup.NoAutoClose : T.Popup.CloseOnEscape
    Connections { target:root.macroModel; function onViewChanged() { if (!root.macroModel.available) root.authorityLost = true } }
    contentItem:T.ScrollView {
        clip:true; contentWidth:availableWidth
        implicitHeight:body.implicitHeight
        ColumnLayout {
            id:body
            width:parent.width
            spacing:Tokens.space["2"]
            TextField { id:name; objectName:"inputMacroName"; Layout.fillWidth:true; text:root.initialMacro.name ?? ""; maximumLength:80; accessibleName:qsTr("Macro name"); placeholderText:qsTr("Macro name") }
            FormRow {
                Layout.fillWidth:true
                label:qsTr("Keyboard trigger")
                description:qsTr("Uses the desktop shortcut registry; conflicts are reported by the desktop")
                editor:ShortcutCaptureButton {
                    id:shortcut
                    objectName:"inputMacroShortcut"
                    sequence:root.macroModel.sequenceKey(root.initialMacro.shortcut ?? "")
                    formatter:key => root.macroModel.sequenceText(key)
                    Accessible.name:qsTr("Macro keyboard shortcut")
                    onCaptured:key => sequence = key
                    onCleared:sequence = 0
                }
            }
            FormRow {
                Layout.fillWidth:true
                label:qsTr("Mouse trigger")
                editor:ComboBox {
                    id:mouse
                    objectName:"inputMacroMouseTrigger"
                    readonly property var choices:[{value:"", label:qsTr("None")}, {value:"left", label:qsTr("Left button")}, {value:"right", label:qsTr("Right button")}, {value:"middle", label:qsTr("Middle button")}, {value:"back", label:qsTr("Back button")}, {value:"forward", label:qsTr("Forward button")}]
                    model:choices; textRole:"label"; valueRole:"value"
                    currentIndex:Math.max(0, choices.findIndex(item => item.value === root.initialMacro.mouseButton))
                    Accessible.name:qsTr("Macro mouse button")
                }
            }
            Flow {
                Layout.fillWidth:true
                visible:(mouse.currentValue ?? "").length > 0
                Repeater {
                    model:["Ctrl","Alt","Shift","Meta"]
                    delegate:CheckBox {
                        required property string modelData
                        text:modelData
                        checked:root.mouseModifiers.includes(modelData)
                        onToggled:{ root.mouseModifiers = checked ? root.mouseModifiers.concat([modelData]).filter((value,index,array) => array.indexOf(value) === index) : root.mouseModifiers.filter(value => value !== modelData); checked = Qt.binding(() => root.mouseModifiers.includes(modelData)) }
                    }
                }
            }
            SectionHeader { Layout.fillWidth:true; title:qsTr("Steps (%1 / 64)").arg(root.steps.length) }
            Repeater {
                model:root.steps
                delegate:RowLayout {
                    required property var modelData
                    required property int index
                    Layout.fillWidth:true
                    Label { text:String(index + 1); muted:true }
                    Button { Layout.fillWidth:true; text:root.macroModel.stepSummary(modelData); onClicked:root.editStep(index); Accessible.name:qsTr("Edit step %1: %2").arg(index + 1).arg(text) }
                    Button { text:"↑"; enabled:index > 0; Accessible.name:qsTr("Move step up"); onClicked:root.moveStep(index,-1) }
                    Button { text:"↓"; enabled:index + 1 < root.steps.length; Accessible.name:qsTr("Move step down"); onClicked:root.moveStep(index,1) }
                    Button { text:qsTr("Copy"); enabled:root.steps.length < 64; onClicked:{ const next = root.steps.slice(); next.splice(index + 1, 0, JSON.parse(JSON.stringify(modelData))); root.steps = next } }
                    Button { text:"×"; Accessible.name:qsTr("Delete step %1").arg(index + 1); onClicked:root.steps = root.steps.filter((step,position) => position !== index) }
                }
            }
            RowLayout {
                Layout.fillWidth:true
                Button { objectName:"inputMacroAddStep"; text:qsTr("Add step…"); enabled:root.steps.length < 64; onClicked:root.editStep(-1) }
                Button { text:qsTr("Add wait"); enabled:root.steps.length < 64; onClicked:root.steps = root.steps.concat([{kind:"wait", milliseconds:150}]) }
            }
            Rectangle {
                id:recorder
                objectName:"inputMacroRecordPad"
                Layout.fillWidth:true; Layout.preferredHeight:110
                property bool recording:false
                color:Tokens.bg.raised; radius:Tokens.radius.s
                border.width:1; border.color:activeFocus ? Tokens.focus.ring : Tokens.outline.strong
                activeFocusOnTab:true
                Label { anchors.centerIn:parent; width:parent.width - 24; horizontalAlignment:Text.AlignHCenter; wrapMode:Text.Wrap; text:recorder.recording ? qsTr("Recording key taps and mouse clicks here. Use Stop recording below when finished.") : qsTr("Click here to record key taps and mouse clicks into the step list.") }
                MouseArea {
                    anchors.fill:parent
                    acceptedButtons:Qt.AllButtons
                    onPressed:mouse => {
                        recorder.forceActiveFocus()
                        if (!recorder.recording) { recorder.recording = true; return }
                        const names = {}; names[Qt.LeftButton] = "left"; names[Qt.RightButton] = "right"; names[Qt.MiddleButton] = "middle"; names[Qt.BackButton] = "back"; names[Qt.ForwardButton] = "forward"
                        if (names[mouse.button] !== undefined) test.recordMouse(names[mouse.button], mouse.modifiers)
                    }
                    // Keep modifiers and key naming in the same codec as manual steps.
                    QtObject {
                        id:test
                        function recordMouse(button, mods) {
                            let modifiers=[]
                            if (mods & Qt.ControlModifier) modifiers.push("Ctrl")
                            if (mods & Qt.AltModifier) modifiers.push("Alt")
                            if (mods & Qt.ShiftModifier) modifiers.push("Shift")
                            if (mods & Qt.MetaModifier) modifiers.push("Meta")
                            root.addRecorded({action:"combination", modifiers:modifiers, keys:[], mouseButtons:[button]})
                        }
                    }
                }
                Keys.priority:Keys.BeforeItem
                Keys.onShortcutOverride:event => { if (recorder.recording) event.accepted = true }
                Keys.onPressed:event => {
                    if (!recorder.recording) return
                    event.accepted = true
                    if (event.isAutoRepeat || [Qt.Key_Control,Qt.Key_Alt,Qt.Key_Shift,Qt.Key_Meta,Qt.Key_AltGr].includes(event.key)) return
                    const binding = root.macroModel.combination({action:"shortcut", shortcut:root.macroModel.portableKey(event.key | event.modifiers)})
                    if (root.macroModel.bindingError(binding).length === 0) root.addRecorded(binding)
                }
            }
            Button { visible:recorder.recording; text:qsTr("Stop recording"); onClicked:recorder.recording = false }
            Label {
                Layout.fillWidth:true
                text:qsTr("Use hold/release steps for drags or overlapping chords. Playback follows the current window; unrelated focus changes cancel it. Physical Escape cancels playback. Save, then Test gives you three seconds to focus the target app.")
                wrapMode:Text.Wrap; muted:true
            }
            Label { objectName:"inputMacroValidation"; Layout.fillWidth:true; text:root.stale ? qsTr("The saved macro changed or the desktop reconnected. Reopen this editor.") : root.failure.length > 0 ? root.failure : root.macroModel.macroError(root.value); visible:text.length > 0; wrapMode:Text.Wrap; Accessible.role:Accessible.AlertMessage }
            RowLayout {
                Layout.fillWidth:true
                Item { Layout.fillWidth:true }
                Button { text:qsTr("Cancel"); onClicked:root.close() }
                Button {
                    objectName:"inputMacroSave"
                    text:qsTr("Save macro"); emphasized:true
                    enabled:!root.stale && root.macroModel.available && !root.macroModel.busy && root.macroModel.macroError(root.value).length === 0
                    onClicked:{ if (root.macroModel.save(root.value, root.expected)) { root.saved(root.initialMacro.id); root.close() } else root.failure = qsTr("The macro could not be saved. Check the steps or reopen the editor.") }
                }
            }
        }
    }
    Loader {
        active:root.stepEditing
        sourceComponent:MacroStepEditor {
            macroModel:root.macroModel
            initialStep:root.stepDraft
            Component.onCompleted:open()
            onSaved:step => root.updateStep(step)
            onClosed:root.stepEditing = false
        }
    }
}

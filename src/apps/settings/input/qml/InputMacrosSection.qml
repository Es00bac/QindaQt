// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id:root
    required property var inputSettings
    readonly property var macros:inputSettings.macros
    property bool editing:false
    property var draft:({})
    property string expected:""
    property string deleting:""
    readonly property Item firstFocusTarget:macros.available ? createButton : retryButton
    signal captureActivityChanged(bool active)
    spacing:Tokens.space["2"]
    Component.onCompleted:macros.refresh()
    function edit(value, existing) {
        root.draft = JSON.parse(JSON.stringify(value))
        delete root.draft.shortcutStatus; delete root.draft.actualShortcuts
        if (existing && ["ready","unassigned"].includes(value.shortcutStatus)) root.draft.shortcut = value.actualShortcuts?.[0] ?? ""
        root.expected = existing ? macros.fingerprint(value.id) : ""
        root.editing = true
    }
    SectionHeader { Layout.fillWidth:true; title:qsTr("Input macros") }
    Label { Layout.fillWidth:true; text:qsTr("Create reusable sequences of keys, clicks, text, movement and delays. Run the same macro from controller buttons, a keyboard shortcut or a mouse combination."); wrapMode:Text.Wrap }
    DegradedNotice { Layout.fillWidth:true; visible:!root.macros.available; reason:root.macros.errorText }
    Button { id:retryButton; visible:!root.macros.available; text:qsTr("Refresh macros"); onClicked:root.macros.refresh() }
    Label { Layout.fillWidth:true; text:root.macros.errorText; visible:text.length > 0 && root.macros.available; wrapMode:Text.Wrap; Accessible.role:Accessible.AlertMessage }
    RowLayout {
        Layout.fillWidth:true; visible:root.macros.available
        Button { id:createButton; objectName:"inputMacroNew"; text:qsTr("New macro…"); enabled:!root.macros.busy && root.macros.macros.length < 64; onClicked:root.edit(root.macros.create(), false) }
        Button { text:qsTr("Copy / paste preset"); enabled:!root.macros.busy; onClicked:root.edit(root.macros.create("copy-paste"), false) }
        Button { text:qsTr("Window drag preset"); enabled:!root.macros.busy; onClicked:root.edit(root.macros.create("drag"), false) }
        Button { text:qsTr("Stop playback"); visible:root.macros.running.length > 0; onClicked:root.macros.cancel() }
    }
    Repeater {
        model:root.macros.macros
        delegate:ColumnLayout {
            required property var modelData
            Layout.fillWidth:true
            RowLayout {
                Layout.fillWidth:true
                Label { Layout.fillWidth:true; text:modelData.name; font.weight:Font.DemiBold; textFormat:Text.PlainText }
                Button { text:qsTr("Edit…"); enabled:!root.macros.busy; onClicked:root.edit(modelData, true) }
                Button { text:qsTr("Test in 3s"); enabled:!root.macros.busy; onClicked:root.macros.run(modelData.id) }
                Button { text:qsTr("Delete…"); enabled:!root.macros.busy; onClicked:{ root.deleting = modelData.id; remove.open() } }
            }
            Label {
                Layout.fillWidth:true
                text:qsTr("%1 steps · Keyboard: %2 · Mouse: %3").arg(modelData.steps.length).arg(modelData.actualShortcuts?.join(", ") || modelData.shortcut || qsTr("none")).arg(modelData.mouseButton ? modelData.mouseModifiers.concat([modelData.mouseButton]).join(" + ") : qsTr("none"))
                wrapMode:Text.Wrap; muted:true
            }
            Label { visible:modelData.shortcutStatus === "conflict" || (modelData.shortcut && modelData.shortcutStatus === "unavailable"); text:modelData.shortcutStatus === "conflict" ? qsTr("The keyboard trigger conflicts with another desktop shortcut. Edit its trigger or use Input → Shortcuts.") : qsTr("The native shortcut service is unavailable. Controller and mouse triggers remain available."); Layout.fillWidth:true; wrapMode:Text.Wrap }
        }
    }
    Loader {
        active:root.editing
        sourceComponent:InputMacroEditor {
            macroModel:root.macros; initialMacro:root.draft; expected:root.expected
            Component.onCompleted:{ open(); root.captureActivityChanged(true) }
            onClosed:{ root.captureActivityChanged(false); root.editing = false }
        }
    }
    T.Dialog {
        id:remove
        parent:T.Overlay.overlay; anchors.centerIn:parent
        modal:true; title:qsTr("Delete macro?"); standardButtons:T.Dialog.Cancel | T.Dialog.Ok
        contentItem:Label { text:qsTr("Controller bindings referencing this macro will stop running it."); wrapMode:Text.Wrap }
        onAccepted:root.macros.remove(root.deleting)
    }
}

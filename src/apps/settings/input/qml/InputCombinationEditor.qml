// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root
    required property var bindingModel
    required property var initialBinding
    property var modifiers: Array.from(initialBinding.modifiers ?? [])
    property var keys: Array.from(initialBinding.keys ?? [])
    property bool showLayer: true
    property string holdPage: initialBinding.holdPage ?? ""
    property var mouseButtons: Array.from(initialBinding.mouseButtons ?? [])
    property string keyError: ""
    readonly property var value: ({action: "combination", modifiers: modifiers, keys: keys, mouseButtons: mouseButtons, holdPage: holdPage})
    spacing: Tokens.space["2"]

    function toggle(list, value, checked) {
        return checked ? list.includes(value) ? list : list.concat([value]) : list.filter(item => item !== value)
    }
    function addKey(key, mods) {
        if (root.keys.length >= 8) { root.keyError = qsTr("Use up to eight keys in a combination."); return }
        let nextMods = root.modifiers.slice()
        if (mods & Qt.ControlModifier) nextMods = root.toggle(nextMods, "Ctrl", true)
        if (mods & Qt.AltModifier) nextMods = root.toggle(nextMods, "Alt", true)
        if (mods & Qt.ShiftModifier) nextMods = root.toggle(nextMods, "Shift", true)
        if (mods & Qt.MetaModifier) nextMods = root.toggle(nextMods, "Meta", true)
        const name = root.bindingModel.portableKey(key)
        const nextKeys = root.keys.includes(name) ? root.keys : root.keys.concat([name])
        const error = root.bindingModel.bindingError({action: "combination", modifiers: nextMods, keys: nextKeys, mouseButtons: root.mouseButtons})
        root.keyError = error
        if (error.length > 0) return
        root.keys = nextKeys; root.modifiers = nextMods
    }
    Label { visible: root.showLayer; text: qsTr("Temporary button layer"); font.weight: Font.DemiBold }
    ComboBox {
        objectName: "controllerCombinationLayer"
        visible: root.showLayer
        Layout.fillWidth: true
        readonly property var choices: [{id:"", name:qsTr("Keep the current button map")}].concat(root.bindingModel.pages ?? [])
        model: choices
        textRole: "name"; valueRole: "id"
        currentIndex: Math.max(0, choices.findIndex(page => page.id === root.holdPage))
        Accessible.name: qsTr("Profile to use while this button is held")
        onActivated: index => { root.holdPage = choices[index].id; currentIndex = Qt.binding(() => Math.max(0, choices.findIndex(page => page.id === root.holdPage))) }
    }
    Label { text: qsTr("Modifiers"); font.weight: Font.DemiBold }
    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]
        Repeater {
            model: ["Ctrl", "Alt", "Shift", "Meta"]
            delegate: CheckBox {
                required property string modelData
                text: modelData
                checked: root.modifiers.includes(modelData)
                onToggled: {
                    root.modifiers = root.toggle(root.modifiers, modelData, checked)
                    checked = Qt.binding(() => root.modifiers.includes(modelData))
                }
            }
        }
    }
    Label { text: qsTr("Mouse buttons"); font.weight: Font.DemiBold }
    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]
        Repeater {
            model: [{value:"left", label:qsTr("Left")}, {value:"right", label:qsTr("Right")}, {value:"middle", label:qsTr("Middle")}, {value:"back", label:qsTr("Back")}, {value:"forward", label:qsTr("Forward")}]
            delegate: CheckBox {
                required property var modelData
                text: modelData.label
                checked: root.mouseButtons.includes(modelData.value)
                onToggled: {
                    root.mouseButtons = root.toggle(root.mouseButtons, modelData.value, checked)
                    checked = Qt.binding(() => root.mouseButtons.includes(modelData.value))
                }
            }
        }
    }
    Label { text: qsTr("Keys"); font.weight: Font.DemiBold }
    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]
        Repeater {
            model: root.keys
            delegate: Button { emphasized: false;
                required property string modelData
                text: modelData + " ×"
                Accessible.name: qsTr("Remove key %1").arg(modelData)
                onClicked: root.keys = root.keys.filter(key => key !== modelData)
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { emphasized: false;
            id: record
            objectName: "controllerRecordKey"
            property bool capturing: false
            text: capturing ? qsTr("Press any key…") : qsTr("Record a key")
            onClicked: { capturing = true; forceActiveFocus() }
            onActiveFocusChanged: if (!activeFocus) capturing = false
            Keys.priority: Keys.BeforeItem
            Keys.onShortcutOverride: event => { if (capturing) event.accepted = true }
            Keys.onPressed: event => {
                if (!capturing) return
                event.accepted = true
                if ([Qt.Key_Control, Qt.Key_Alt, Qt.Key_Shift, Qt.Key_Meta, Qt.Key_AltGr].includes(event.key)) return
                root.addKey(event.key, event.modifiers)
                capturing = false
            }
        }
        Button { emphasized: false; visible: record.capturing; text: qsTr("Cancel recording"); onClicked: record.capturing = false }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: namedKey
            objectName: "controllerNamedKey"
            Layout.fillWidth: true
            placeholderText: qsTr("Or name a key: Esc, Tab, Space, F13…")
            accessibleName: qsTr("Key name")
            onAccepted: addNamed.clicked()
        }
        Button { emphasized: false;
            id: addNamed
            text: qsTr("Add key")
            onClicked: {
                const key = root.bindingModel.sequenceKey(namedKey.text)
                root.addKey(key & ~0xfe000000, key & 0xfe000000)
                if (root.keyError.length === 0) namedKey.clear()
            }
        }
    }
    Label {
        Layout.fillWidth: true
        text: root.keyError.length > 0 ? root.keyError : root.showLayer ? qsTr("Modifiers, keys and mouse buttons stay held until you release the controller button. Esc and Tab can also be recorded.") : qsTr("Modifiers, keys and mouse buttons can be combined freely. A tap releases automatically; a hold needs a matching release step.")
        wrapMode: Text.Wrap
        muted: true
    }
}

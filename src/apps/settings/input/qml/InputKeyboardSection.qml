// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Keyboard rows: repeat behavior with a test field, NumLock at login, and
// the configured layout list. Layout edits apply through the model's
// apply() so the status line tells the exact storage/reload outcome.
ColumnLayout {
    id: root

    required property var inputSettings
    readonly property var keyboard: inputSettings.keyboard
    readonly property var layouts: inputSettings.layouts
    readonly property bool keyboardUsable: keyboard.available

    readonly property Item firstFocusTarget: degraded.visible ? degraded
                                             : repeatSwitch

    Component.onCompleted: {
        keyboard.refresh()
        layouts.refresh()
    }

    spacing: Tokens.space["2"]

    DegradedNotice {
        id: degraded
        objectName: "inputKeyboardDegraded"
        Layout.fillWidth: true
        visible: !keyboardUsable
        reason: qsTr("Keyboard settings are unavailable right now.")
    }

    FormRow {
        objectName: "inputKeyRepeatRow"
        Layout.fillWidth: true
        visible: keyboardUsable
        label: qsTr("Key repeat")
        description: qsTr("Holding a key keeps typing it")
        editor: Switch {
            id: repeatSwitch
            objectName: "inputKeyRepeatSwitch"
            checked: keyboard.keyRepeat
            onToggled: keyboard.keyRepeat = checked
        }
    }

    FormRow {
        objectName: "inputRepeatDelayRow"
        Layout.fillWidth: true
        visible: keyboardUsable && keyboard.keyRepeat
        label: qsTr("Delay")
        description: qsTr("How long a key is held before it repeats")
        editor: Slider {
            objectName: "inputRepeatDelaySlider"
            from: 100
            to: 2000
            stepSize: 10
            value: keyboard.repeatDelayMs
            onMoved: keyboard.repeatDelayMs = value
            accessibleName: qsTr("Repeat delay")
        }
    }

    FormRow {
        objectName: "inputRepeatRateRow"
        Layout.fillWidth: true
        visible: keyboardUsable && keyboard.keyRepeat
        label: qsTr("Rate")
        description: qsTr("How fast the key repeats once it starts")
        editor: Slider {
            objectName: "inputRepeatRateSlider"
            from: 1
            to: 100
            stepSize: 1
            value: keyboard.repeatRate
            onMoved: keyboard.repeatRate = value
            accessibleName: qsTr("Repeat rate")
        }
    }

    FormRow {
        objectName: "inputRepeatTestRow"
        Layout.fillWidth: true
        visible: keyboardUsable && keyboard.keyRepeat
        label: qsTr("Test")
        description: qsTr("Hold a key here to feel the current delay and rate")
        editor: TextField {
            objectName: "inputRepeatTestField"
            placeholderText: qsTr("Type here to test")
            Layout.preferredWidth: 320
        }
    }

    FormRow {
        objectName: "inputNumLockRow"
        Layout.fillWidth: true
        visible: keyboardUsable
        label: qsTr("NumLock at login")
        description: qsTr("State of the number pad when the session starts")
        editor: ComboBox {
            objectName: "inputNumLockCombo"
            Layout.preferredWidth: 220
            textRole: "label"
            model: [
                { label: qsTr("Off"), value: 0 },
                { label: qsTr("On"), value: 1 },
                { label: qsTr("Keep current"), value: 2 }
            ]
            valueRole: "value"
            currentIndex: indexOfValue(keyboard.numLockAtLogin)
            onActivated: index => keyboard.numLockAtLogin = currentValue
            function indexOfValue(value) {
                for (let i = 0; i < model.length; ++i)
                    if (model[i].value === value) return i
                return 2
            }
        }
    }

    SectionHeader {
        objectName: "inputKeyboardLayoutsHeader"
        Layout.fillWidth: true
        visible: keyboardUsable
        title: qsTr("Keyboard layouts")
    }

    Label {
        objectName: "inputLayoutsStatus"
        Layout.fillWidth: true
        visible: text.length > 0
        text: layouts.statusText
        wrapMode: Text.Wrap
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    ListView {
        objectName: "inputLayoutsList"
        Layout.fillWidth: true
        Layout.preferredHeight: contentHeight
        visible: keyboardUsable && layouts.count > 0
        interactive: false
        clip: true
        model: layouts
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Configured keyboard layouts")

        delegate: T.ItemDelegate {
            id: layoutRow
            required property int index
            required property string title
            required property string variantTitle
            width: ListView.view.width

            contentItem: RowLayout {
                spacing: Tokens.space["2"]
                Label {
                    Layout.fillWidth: true
                    text: layoutRow.variantTitle.length > 0
                          ? qsTr("%1 (%2)").arg(layoutRow.title)
                                              .arg(layoutRow.variantTitle)
                          : layoutRow.title
                }
                Button {
                    objectName: "inputLayoutMoveUp_" + layoutRow.index
                    icon.name: "go-up"
                    text: qsTr("Move up")
                    enabled: layoutRow.index > 0
                    onClicked: layouts.moveUp(layoutRow.index)
                }
                Button {
                    objectName: "inputLayoutMoveDown_" + layoutRow.index
                    icon.name: "go-down"
                    text: qsTr("Move down")
                    enabled: layoutRow.index < layouts.count - 1
                    onClicked: layouts.moveDown(layoutRow.index)
                }
                Button {
                    objectName: "inputLayoutRemove_" + layoutRow.index
                    icon.name: "list-remove"
                    text: qsTr("Remove")
                    enabled: layouts.count > 1
                    onClicked: layouts.removeRow(layoutRow.index)
                }
            }
        }
    }

    RowLayout {
        objectName: "inputLayoutAddRow"
        Layout.fillWidth: true
        visible: keyboardUsable && layouts.catalog.count > 0
        spacing: Tokens.space["2"]

        ComboBox {
            id: layoutPicker
            objectName: "inputLayoutAddLayout"
            Layout.preferredWidth: 220
            textRole: "description"
            valueRole: "code"
            model: layouts.catalog
            Accessible.name: qsTr("Layout to add")
            onCurrentIndexChanged: refreshVariants()
            function refreshVariants() {
                const row = layouts.catalog.layoutAt(currentIndex)
                variantPicker.variants =
                    row !== undefined && !row.isEmpty ? row.variants : []
                variantPicker.currentIndex =
                    variantPicker.variants.length > 0 ? 0 : -1
            }
        }
        ComboBox {
            id: variantPicker
            objectName: "inputLayoutAddVariant"
            Layout.preferredWidth: 220
            enabled: variants.length > 0
            property var variants: []
            textRole: "description"
            valueRole: "code"
            model: variants
            Accessible.name: qsTr("Variant")
        }
        Button {
            objectName: "inputLayoutAddButton"
            text: qsTr("Add layout")
            enabled: layoutPicker.currentIndex >= 0
            onClicked: layouts.addLayout(layoutPicker.currentValue ?? "",
                                         variantPicker.variants.length > 0
                                             ? variantPicker.currentValue ?? ""
                                             : "")
        }
        Button {
            objectName: "inputLayoutsApplyButton"
            emphasized: true
            text: qsTr("Apply")
            onClicked: layouts.apply()
        }
    }

    Item { Layout.fillHeight: true }
}

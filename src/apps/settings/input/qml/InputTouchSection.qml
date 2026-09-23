// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Touch rows (ADR-0205): the touchscreen switch, how long a finger is held
// for the menu, the on-screen keyboard, and what a swipe from each screen
// edge opens. Rows retain last-known values when authority
// is lost; edits wait for current authority and confirmed write readback.
ColumnLayout {
    id: root

    required property var inputSettings
    readonly property var touch: inputSettings.touch
    readonly property bool touchUsable: touch.available
    readonly property bool touchKnown: touch.hasLastKnown
    readonly property Item firstFocusTarget: degraded.visible ? degraded : touchSwitch

    Component.onCompleted: touch.refresh()

    spacing: Tokens.space["2"]

    DegradedNotice {
        id: degraded
        objectName: "inputTouchDegraded"
        Layout.fillWidth: true
        visible: !root.touchUsable
        reason: root.touch.errorText.length > 0 ? root.touch.errorText : root.touch.statusText
    }

    FormRow {
        objectName: "inputTouchEnabledRow"
        Layout.fillWidth: true
        visible: root.touchKnown
        label: qsTr("Touchscreen")
        description: qsTr("Fingers on the screen move, tap and hold")
        editor: Switch {
            id: touchSwitch
            objectName: "inputTouchEnabledSwitch"
            checked: root.touch.touchscreenEnabled
            enabled: root.touch.editable
            onToggled: {
                root.touch.setTouchscreenEnabled(checked)
                checked = Qt.binding(() => root.touch.touchscreenEnabled)
            }
        }
    }

    FormRow {
        objectName: "inputTouchLongPressRow"
        Layout.fillWidth: true
        visible: root.touchKnown && root.touch.touchscreenEnabled
        label: qsTr("Hold for menu")
        description: qsTr("How long a finger stays down before the menu opens")
        editor: Slider {
            objectName: "inputTouchLongPressSlider"
            from: 200
            to: 1500
            stepSize: 50
            value: root.touch.longPressDisplayMs
            enabled: root.touch.editable || root.touch.longPressQueueable
            onMoved: {
                if (!root.touch.setLongPressMs(Math.round(value)))
                    value = Qt.binding(() => root.touch.longPressDisplayMs)
            }
            accessibleName: qsTr("Hold time for the menu")
        }
    }

    FormRow {
        objectName: "inputTouchKeyboardRow"
        Layout.fillWidth: true
        visible: root.touchKnown && root.touch.touchscreenEnabled
        label: qsTr("On-screen keyboard")
        description: qsTr("Shown when a finger focuses a text field")
        editor: ComboBox {
            objectName: "inputTouchKeyboardCombo"
            width: 220
            textRole: "label"
            valueRole: "value"
            model: root.touch.keyboardChoices
            currentIndex: root.touch.choiceIndex(root.touch.keyboardChoices, root.touch.onScreenKeyboard)
            enabled: root.touch.editable
            onActivated: index => {
                root.touch.setOnScreenKeyboard(root.touch.keyboardChoices[index].value)
                currentIndex = Qt.binding(() => root.touch.choiceIndex(
                                              root.touch.keyboardChoices, root.touch.onScreenKeyboard))
            }
        }
    }

    SectionHeader {
        objectName: "inputTouchEdgesHeader"
        Layout.fillWidth: true
        visible: root.touchKnown && root.touch.touchscreenEnabled
        title: qsTr("Edge swipes")
    }

    Repeater {
        model: root.touchKnown && root.touch.touchscreenEnabled
               ? [{ edge: "left", label: qsTr("From the left") },
                  { edge: "top", label: qsTr("From the top") },
                  { edge: "right", label: qsTr("From the right") },
                  { edge: "bottom", label: qsTr("From the bottom") }]
               : []
        delegate: FormRow {
            required property var modelData
            objectName: "inputTouchEdgeRow_" + modelData.edge
            Layout.fillWidth: true
            label: modelData.label
            description: qsTr("What a finger swiped in from this edge opens")
            editor: ComboBox {
                objectName: "inputTouchEdgeCombo_" + modelData.edge
                width: 220
                textRole: "label"
                valueRole: "value"
                model: root.touch.edgeActionChoices
                currentIndex: root.touch.choiceIndex(root.touch.edgeActionChoices,
                                                     root.touch.edgeAction(modelData.edge))
                enabled: root.touch.editable
                onActivated: index => {
                    root.touch.setEdgeAction(modelData.edge, root.touch.edgeActionChoices[index].value)
                    currentIndex = Qt.binding(() => root.touch.choiceIndex(
                                                  root.touch.edgeActionChoices,
                                                  root.touch.edgeAction(modelData.edge)))
                }
            }
        }
    }

    Label {
        objectName: "inputTouchStatus"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.touch.errorText.length > 0 ? root.touch.errorText : root.touch.statusText
        wrapMode: Text.Wrap
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}

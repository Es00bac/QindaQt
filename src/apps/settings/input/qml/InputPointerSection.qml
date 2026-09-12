// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Mouse & touchpad rows. Rows a device cannot honor are hidden through the
// selection's availability truth; nothing renders disabled-but-visible for
// unsupported capabilities (ADR-0134).
ColumnLayout {
    id: root

    required property var inputSettings
    readonly property var selection: inputSettings.pointerDevices.selection
    readonly property bool hasDevices: inputSettings.pointerDevices.count > 0

    readonly property Item firstFocusTarget: degraded.visible ? degraded
                                             : devicePicker.visible ? devicePicker
                                             : noDevices.visible ? noDevices
                                             : speedRow.editor

    Component.onCompleted: inputSettings.pointerDevices.refresh()

    spacing: Tokens.space["2"]

    Label {
        visible: text.length > 0
        text: root.selection !== null ? root.selection.statusText : ""
        wrapMode: Text.Wrap
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    DegradedNotice {
        id: degraded
        objectName: "inputPointerDegraded"
        Layout.fillWidth: true
        visible: !inputSettings.pointerDevices.available
        reason: qsTr("Pointer devices are unavailable right now.")
    }

    Label {
        id: noDevices
        objectName: "inputPointerNoDevices"
        Layout.fillWidth: true
        visible: inputSettings.pointerDevices.available && !root.hasDevices
        text: qsTr("No pointer or touchpad devices found.")
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    FormRow {
        id: devicePicker
        objectName: "inputPointerDevicePicker"
        Layout.fillWidth: true
        visible: root.hasDevices && inputSettings.pointerDevices.count > 1
        label: qsTr("Device")
        description: qsTr("Choose which pointer device these settings change")
        editor: ComboBox {
            objectName: "inputPointerDeviceCombo"
            textRole: "label"
            model: inputSettings.pointerDevices
            width: 320
            onActivated: index => inputSettings.pointerDevices.select(index)
        }
    }

    // AGENT-GUARD: The combo's selected index is the model's truth, not the
    // combo's: QQuickComboBox keeps currentIndex at -1 when its model resets
    // into non-empty, so without this binding the picker shows no device even
    // though the rows describe the first one. A Binding element survives the
    // combo's own imperative write on popup selection, which would break a
    // plain property binding for the rest of the session.
    Binding {
        target: devicePicker.editor
        property: "currentIndex"
        value: inputSettings.pointerDevices.selectedIndex
        restoreMode: Binding.RestoreBindingOrValue
    }

    FormRow {
        id: speedRow
        objectName: "inputPointerSpeedRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.speedAvailable
        label: qsTr("Pointer speed")
        description: qsTr("How far the pointer moves for the same hand motion")
        editor: Slider {
            objectName: "inputPointerSpeedSlider"
            id: speedSlider
            from: -1.0
            to: 1.0
            stepSize: 0.05
            value: root.selection !== null ? root.selection.speed : 0.0
            onMoved: root.selection.speed = value
            accessibleName: qsTr("Pointer speed")
        }
    }

    FormRow {
        objectName: "inputPointerProfileRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.profileAvailable
        label: qsTr("Acceleration")
        description: qsTr("Flat keeps one speed; adaptive speeds up with faster motion")
        editor: ComboBox {
            objectName: "inputPointerProfileCombo"
            width: 220
            textRole: "label"
            model: [
                { label: qsTr("Adaptive"), value: false },
                { label: qsTr("Flat"), value: true }
            ]
            valueRole: "value"
            currentIndex: root.selection !== null && root.selection.flatProfile ? 1 : 0
            onActivated: index => root.selection.flatProfile =
                             currentIndex === 1
        }
    }

    FormRow {
        objectName: "inputPointerNaturalScrollRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.naturalScrollAvailable
        label: qsTr("Natural scrolling")
        description: qsTr("Content moves in the same direction as the fingers")
        editor: Switch {
            objectName: "inputPointerNaturalScrollSwitch"
            checked: root.selection !== null && root.selection.naturalScroll
            onToggled: root.selection.naturalScroll = checked
        }
    }

    FormRow {
        objectName: "inputPointerLeftHandedRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.leftHandedAvailable
        label: qsTr("Left-handed")
        description: qsTr("Swap the primary and secondary mouse buttons")
        editor: Switch {
            objectName: "inputPointerLeftHandedSwitch"
            checked: root.selection !== null && root.selection.leftHanded
            onToggled: root.selection.leftHanded = checked
        }
    }

    FormRow {
        objectName: "inputPointerScrollSpeedRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.scrollSpeedAvailable
        label: qsTr("Scroll speed")
        description: qsTr("How many lines one wheel step moves")
        editor: Slider {
            objectName: "inputPointerScrollSpeedSlider"
            from: 0.1
            to: 5.0
            stepSize: 0.1
            value: root.selection !== null ? root.selection.scrollSpeed : 1.0
            onMoved: root.selection.scrollSpeed = value
            accessibleName: qsTr("Scroll speed")
        }
    }

    FormRow {
        objectName: "inputPointerMiddleEmulationRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.middleEmulationAvailable
        label: qsTr("Middle-click emulation")
        description: qsTr("Pressing both buttons acts as a middle click")
        editor: Switch {
            objectName: "inputPointerMiddleEmulationSwitch"
            checked: root.selection !== null && root.selection.middleEmulation
            onToggled: root.selection.middleEmulation = checked
        }
    }

    SectionHeader {
        objectName: "inputTouchpadHeader"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.tapToClickAvailable
        title: qsTr("Touchpad")
    }

    FormRow {
        objectName: "inputTouchpadTapToClickRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.tapToClickAvailable
        label: qsTr("Tap to click")
        description: qsTr("A short tap acts as a button press")
        editor: Switch {
            objectName: "inputTouchpadTapToClickSwitch"
            checked: root.selection !== null && root.selection.tapToClick
            onToggled: root.selection.tapToClick = checked
        }
    }

    FormRow {
        objectName: "inputTouchpadTapAndDragRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.tapAndDragAvailable
        label: qsTr("Tap and drag")
        description: qsTr("Tap twice and hold to drag items")
        editor: Switch {
            objectName: "inputTouchpadTapAndDragSwitch"
            checked: root.selection !== null && root.selection.tapAndDrag
            onToggled: root.selection.tapAndDrag = checked
        }
    }

    FormRow {
        objectName: "inputTouchpadDisableWhileTypingRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.disableWhileTypingAvailable
        label: qsTr("Disable while typing")
        description: qsTr("Ignores palm touches while the keyboard is in use")
        editor: Switch {
            objectName: "inputTouchpadDisableWhileTypingSwitch"
            checked: root.selection !== null && root.selection.disableWhileTyping
            onToggled: root.selection.disableWhileTyping = checked
        }
    }

    FormRow {
        objectName: "inputTouchpadScrollMethodRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.scrollMethodAvailable
        label: qsTr("Scroll method")
        description: qsTr("Two fingers beside each other, or one finger along the edge")
        editor: ComboBox {
            objectName: "inputTouchpadScrollMethodCombo"
            width: 220
            textRole: "label"
            model: [
                { label: qsTr("Two fingers"), value: "two-finger" },
                { label: qsTr("Edge"), value: "edge" }
            ]
            valueRole: "value"
            currentIndex: root.selection !== null
                          && root.selection.scrollMethod === "edge" ? 1 : 0
            onActivated: index => root.selection.scrollMethod =
                             currentIndex === 1 ? "edge" : "two-finger"
        }
    }

    Item { Layout.fillHeight: true }
}

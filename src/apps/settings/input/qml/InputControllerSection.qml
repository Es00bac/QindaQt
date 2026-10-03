// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root
    required property var inputSettings
    readonly property var controllers: inputSettings.controllers
    readonly property var selected: controllers.selected
    readonly property var config: selected.config ?? ({})
    readonly property bool editable: controllers.available && !controllers.busy
    readonly property bool playstation: selected.family === "playstation"
    readonly property bool hasTouchpad: selected.hasTouchpad ?? (selected.template && playstation)
    readonly property bool hasGyro: selected.hasGyro ?? (selected.template && (playstation || selected.family === "nintendo"))
    readonly property Item firstFocusTarget: controllers.available ? controllerChoice : retryButton
    signal captureActivityChanged(bool active)
    signal audioSettingsRequested()
    Component.onCompleted: controllers.refresh()
    spacing: Tokens.space["2"]

    SectionHeader { Layout.fillWidth: true; title: qsTr("Controllers") }
    Label {
        Layout.fillWidth: true
        text: qsTr("Use Xbox, PlayStation, Nintendo and other game controllers on the desktop. Steam takes priority while it is running; desktop input also pauses when a game uses a controller or an app is fullscreen.")
        wrapMode: Text.Wrap
        Accessible.name: text
    }
    DegradedNotice {
        Layout.fillWidth: true
        visible: !root.controllers.available
        reason: root.controllers.errorText
    }
    Button {
        id: retryButton
        objectName: "controllerRefresh"
        visible: !root.controllers.available
        text: qsTr("Refresh controllers")
        onClicked: root.controllers.refresh()
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available
        label: qsTr("Controller or defaults")
        description: qsTr("Family defaults can be set before a controller is connected")
        editor: ComboBox {
            id: controllerChoice
            objectName: "controllerChoice"
            width: 320
            model: root.controllers.controllers
            textRole: "name"
            valueRole: "id"
            currentIndex: Math.max(0, root.controllers.controllers.findIndex(entry => entry.id === root.controllers.selectedId))
            enabled: !root.controllers.busy
            Accessible.name: qsTr("Controller or family defaults")
            onActivated: index => root.controllers.select(root.controllers.controllers[index].id)
        }
    }
    Label {
        objectName: "controllerStatus"
        Layout.fillWidth: true
        text: root.controllers.errorText.length > 0 ? root.controllers.errorText : root.controllers.statusText
        wrapMode: Text.Wrap
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available
        label: qsTr("Desktop control")
        description: qsTr("Enable this controller’s desktop bindings")
        editor: Switch {
            objectName: "controllerEnabled"
            checked: root.config.enabled ?? true
            enabled: root.editable
            onToggled: {
                root.controllers.setOption("enabled", checked)
                checked = Qt.binding(() => root.config.enabled ?? true)
            }
        }
    }
    SectionHeader { Layout.fillWidth: true; visible: root.controllers.available; title: qsTr("Mouse control") }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available
        label: qsTr("Pointer stick")
        description: qsTr("The other stick scrolls")
        editor: ComboBox {
            objectName: "controllerPointerStick"
            width: 220
            model: [{ value: "left", label: qsTr("Left stick") }, { value: "right", label: qsTr("Right stick") }, { value: "off", label: qsTr("Off") }]
            textRole: "label"; valueRole: "value"
            currentIndex: root.config.pointerStick === "right" ? 1 : root.config.pointerStick === "off" ? 2 : 0
            enabled: root.editable
            onActivated: index => root.controllers.setOption("pointerStick", model[index].value)
        }
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available
        label: qsTr("Pointer speed")
        editor: Slider {
            objectName: "controllerPointerSpeed"
            from: 100; to: 3000; stepSize: 100
            value: root.config.pointerSpeed ?? 1000
            enabled: root.editable
            accessibleName: qsTr("Controller pointer speed")
            onMoved: if (!pressed) root.controllers.setOption("pointerSpeed", value)
            onPressedChanged: if (!pressed && Math.abs(value - (root.config.pointerSpeed ?? 1000)) > 1) root.controllers.setOption("pointerSpeed", value)
        }
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available
        label: qsTr("Stick dead zone")
        description: qsTr("Increase this if the pointer drifts when the stick is centered")
        editor: Slider {
            objectName: "controllerDeadzone"
            from: 0.05; to: 0.6; stepSize: 0.01
            value: root.config.deadzone ?? 0.18
            enabled: root.editable
            accessibleName: qsTr("Controller stick dead zone")
            onMoved: if (!pressed) root.controllers.setOption("deadzone", value)
            onPressedChanged: if (!pressed && Math.abs(value - (root.config.deadzone ?? 0.18)) > 0.005) root.controllers.setOption("deadzone", value)
        }
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available && root.hasTouchpad
        label: qsTr("Touchpad mouse")
        description: qsTr("Slide a finger on the controller touchpad to move the pointer")
        editor: Switch {
            objectName: "controllerTouchpad"
            checked: root.config.touchpad ?? true
            enabled: root.editable
            onToggled: {
                root.controllers.setOption("touchpad", checked)
                checked = Qt.binding(() => root.config.touchpad ?? true)
            }
        }
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available && root.hasGyro
        label: qsTr("Gyro mouse")
        description: qsTr("Tilt the controller to move the pointer; off by default")
        editor: Switch {
            objectName: "controllerGyro"
            checked: root.config.gyro ?? false
            enabled: root.editable
            onToggled: {
                root.controllers.setOption("gyro", checked)
                checked = Qt.binding(() => root.config.gyro ?? false)
            }
        }
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available && root.hasGyro && (root.config.gyro ?? false)
        label: qsTr("Gyro sensitivity")
        editor: Slider {
            objectName: "controllerGyroSpeed"
            from: 50; to: 2000; stepSize: 50
            value: root.config.gyroSpeed ?? 650
            enabled: root.editable
            accessibleName: qsTr("Controller gyro sensitivity")
            onMoved: if (!pressed) root.controllers.setOption("gyroSpeed", value)
            onPressedChanged: if (!pressed && Math.abs(value - (root.config.gyroSpeed ?? 650)) > 1) root.controllers.setOption("gyroSpeed", value)
        }
    }
    FormRow {
        Layout.fillWidth: true
        visible: root.controllers.available && root.playstation
        label: qsTr("Controller microphone")
        description: qsTr("Connect a DualSense by USB and select its input in Audio settings. Dictation uses the selected default microphone.")
        editor: Button { text: qsTr("Audio settings"); onClicked: root.audioSettingsRequested() }
    }
    SectionHeader { Layout.fillWidth: true; visible: root.controllers.available; title: qsTr("Button bindings") }
    Label {
        Layout.fillWidth: true
        visible: root.controllers.available
        text: qsTr("Share / View / Minus holds dictation by default. Shoulders switch windows, the top face button opens the overview, and Menu / Options / Plus opens the launcher. Each button can be changed below.")
        wrapMode: Text.Wrap
        Accessible.name: text
    }
    Repeater {
        model: root.controllers.available ? root.controllers.buttons : []
        delegate: ControllerBindingRow {
            required property var modelData
            Layout.fillWidth: true
            binding: modelData
            controllerModel: root.controllers
            onCaptureActivityChanged: active => root.captureActivityChanged(active)
        }
    }
    Button {
        objectName: "controllerReset"
        visible: root.controllers.available
        text: qsTr("Restore controller defaults")
        enabled: root.editable
        onClicked: root.controllers.reset()
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The per-luminaire settings a device row reveals when it is expanded. Split
// from SmartLightDeviceRow so the row keeps one job — identity, power, and
// disclosure — and this keeps the other: the capability-gated controls.
ColumnLayout {
    id: root

    required property var row
    required property var access
    required property var colors
    required property var swatches
    // Scene options are only asked for while the section is on screen.
    required property bool expanded

    signal swatchChosen(string hex)

    objectName: "smartLightDeviceControls"
    spacing: 6

    Label {
        Layout.fillWidth: true
        visible: !root.row.capabilitiesKnown
        text: qsTr("This light has not said yet what it can do.")
        color: root.colors.textMuted ?? "#a9afa9"
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.row.supportsDimming
        spacing: 8

        Label {
            text: qsTr("Brightness")
            color: root.colors.text ?? "white"
        }

        C.Slider {
            id: brightness
            objectName: "smartLightBrightnessSlider"
            Layout.fillWidth: true
            from: root.row.minimumBrightnessPercent
            to: 100
            stepSize: 1
            enabled: root.row.controllable
            accessibleName: qsTr("Brightness of %1").arg(root.row.label)
            accessibleDescription: qsTr("%1 percent").arg(Math.round(value))
            // AGENT-GUARD: a plain `value:` binding is destroyed the
            // first time the user drags the handle, which would leave
            // the slider frozen while the light changes elsewhere.
            // Rebinding while it is not being touched keeps the control
            // honest about what the light is actually doing.
            onMoved: root.access.requestBrightness(root.row.deviceId,
                                                   Math.round(value))
        }

        Binding {
            target: brightness
            property: "value"
            value: root.row.brightnessPercent
            when: !brightness.pressed
            restoreMode: Binding.RestoreNone
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.row.supportsTemperature
        spacing: 8

        Label {
            text: qsTr("Warmth")
            color: root.colors.text ?? "white"
        }

        C.Slider {
            id: temperature
            objectName: "smartLightTemperatureSlider"
            Layout.fillWidth: true
            from: root.row.minimumKelvin
            to: Math.max(root.row.maximumKelvin, root.row.minimumKelvin + 1)
            stepSize: 50
            enabled: root.row.controllable
            accessibleName: qsTr("White temperature of %1").arg(root.row.label)
            accessibleDescription: qsTr("%1 kelvin").arg(Math.round(value))
            onMoved: root.access.requestTemperature(root.row.deviceId,
                                                    Math.round(value))
        }

        Binding {
            target: temperature
            property: "value"
            value: root.row.temperatureKelvin
            when: !temperature.pressed
            restoreMode: Binding.RestoreNone
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.row.supportsColor
        spacing: 6

        Label {
            text: qsTr("Colour")
            color: root.colors.text ?? "white"
        }

        Repeater {
            model: root.swatches

            delegate: AbstractButton {
                required property string modelData

                objectName: "smartLightColorSwatch"
                implicitWidth: 22
                implicitHeight: 22
                enabled: root.row.controllable
                focusPolicy: Qt.TabFocus
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Set %1 to this colour").arg(root.row.label)
                onClicked: root.swatchChosen(modelData)
                Accessible.onPressAction: root.swatchChosen(modelData)

                background: Rectangle {
                    radius: 4
                    color: parent.modelData
                    opacity: parent.enabled ? 1.0 : 0.4
                    border.width: parent.activeFocus ? 2 : 1
                    border.color: parent.activeFocus
                                  ? Tokens.focus.ring : Tokens.outline.strong
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.row.supportsScenes
        spacing: 8

        Label {
            text: qsTr("Scene")
            color: root.colors.text ?? "white"
        }

        ComboBox {
            id: sceneChoice
            objectName: "smartLightSceneChoice"
            Layout.fillWidth: true
            enabled: root.row.controllable
            textRole: "name"
            valueRole: "sceneId"
            // Only asked for while the section is open: the option list
            // depends on this exact luminaire's proven capabilities.
            model: root.expanded && root.access !== null
                   ? root.access.sceneOptions(root.row.deviceId) : []
            currentIndex: {
                const options = model
                if (!options || options.length === 0)
                    return -1
                for (let index = 0; index < options.length; ++index) {
                    if (options[index].sceneId === root.row.sceneId)
                        return index
                }
                return -1
            }
            Accessible.name: qsTr("Scene for %1").arg(root.row.label)
            onActivated: (index) => {
                const option = model[index]
                if (option !== undefined)
                    root.access.requestScene(root.row.deviceId, option.sceneId)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.row.speedApplies
        spacing: 8

        Label {
            text: qsTr("Speed")
            color: root.colors.text ?? "white"
        }

        C.Slider {
            id: speed
            objectName: "smartLightSpeedSlider"
            Layout.fillWidth: true
            from: 10
            to: 200
            stepSize: 5
            enabled: root.row.controllable
            accessibleName: qsTr("Scene speed of %1").arg(root.row.label)
            accessibleDescription: qsTr("%1 percent").arg(Math.round(value))
            onMoved: root.access.requestSpeed(root.row.deviceId,
                                              Math.round(value))
        }

        Binding {
            target: speed
            property: "value"
            value: root.row.speedPercent
            when: !speed.pressed
            restoreMode: Binding.RestoreNone
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        C.TextField {
            id: rename
            objectName: "smartLightRenameField"
            Layout.fillWidth: true
            placeholderText: root.row.label
            accessibleName: qsTr("New name for %1").arg(root.row.label)
            onAccepted: renameButton.commit()
        }

        C.Button {
            id: renameButton
            objectName: "smartLightRenameButton"
            emphasized: false
            text: qsTr("Rename")
            available: rename.text.trim().length > 0
            accessibleDescription: qsTr("Give %1 a new name").arg(root.row.label)

            function commit() {
                if (!available)
                    return
                if (root.access.requestRename(root.row.deviceId, rename.text))
                    rename.text = ""
            }

            onClicked: commit()
        }

        C.Button {
            objectName: "smartLightForgetButton"
            emphasized: false
            destructive: true
            text: qsTr("Forget")
            accessibleDescription: qsTr("Forget %1 and its saved settings")
                .arg(root.row.label)
            onClicked: root.access.requestForget(root.row.deviceId)
        }
    }
}

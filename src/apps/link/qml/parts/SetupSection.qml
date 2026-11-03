// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import QindaQt.Link

// "Set up phone over USB". The service owns ADB and runs the grants; this section only asks, and
// lists what happened per step.
LinkPanel {
    id: section
    objectName: "setupPanel"
    title: qsTr("Set up phone over USB")
    iconName: "wrench"

    Tk.Flex {
        width: parent.width
        direction: Tk.Flex.Column
        align: Tk.Flex.Stretch
        gap: Tk.Theme.space.sm

        Tk.Caption {
            Tk.Flex.shrink: 0
            Tk.Flex.minWidth: 0
            wrapMode: Text.WordWrap
            text: qsTr("A sideloaded app needs permissions that Android hides behind several Settings screens. "
                       + "With USB debugging on, this grants them in one go: messages, contacts, notifications "
                       + "access and battery exemption.")
        }
        Tk.Flex {
            gap: Tk.Theme.space.xs
            Tk.Button {
                objectName: "setupPhoneButton"
                text: qsTr("Set up phone")
                iconName: "wrench"
                variant: "accent"
                small: true
                busy: Devices.setupBusy
                enabled: Devices.serviceAvailable && !Devices.setupBusy
                onClicked: Devices.setupPhone()
            }
            Tk.Button {
                objectName: "checkSetupButton"
                text: qsTr("Check status")
                iconName: "refresh-cw"
                small: true
                enabled: Devices.serviceAvailable && !Devices.setupBusy
                onClicked: Devices.checkSetup()
            }
        }
        Tk.Label {
            visible: Devices.setupMessage.length > 0
            objectName: "setupMessage"
            text: Devices.setupMessage
            wrapMode: Text.WordWrap
            muted: true
            Tk.Flex.shrink: 0
            Tk.Flex.minWidth: 0
        }
        Tk.Flex {
            direction: Tk.Flex.Column
            align: Tk.Flex.Stretch
            gap: 0
            Repeater {
                model: Devices.setupSteps
                delegate: Tk.ListRow {
                    required property string label
                    required property bool ok
                    required property string outcome
                    small: true
                    text: label
                    iconName: ok ? "circle-check" : "circle-x"
                    trailing: Tk.Caption {
                        text: outcome
                        color: ok ? Tk.Theme.color.success : Tk.Theme.color.danger
                    }
                }
            }
        }
    }
}

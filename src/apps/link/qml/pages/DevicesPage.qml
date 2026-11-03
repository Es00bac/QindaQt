// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import QindaQt.Link
import "../parts" as Parts

// Devices: pairing requests first (they need an answer), then the paired phone and the USB setup.
Tk.Scroll {
    id: page
    objectName: "devicesPage"
    padding: Tk.Theme.space.sm

    Tk.Flex {
        width: page.viewportWidth - page.padding * 2
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Parts.ServiceNotice { Tk.Flex.shrink: 0 }

        Tk.Notice {
            visible: Devices.error.length > 0 && Devices.serviceAvailable
            variant: "danger"
            text: Devices.error
        }

        Repeater {
            model: Devices.pairings
            delegate: Parts.PairingCard {
                required property string id
                required property string name
                required property string model
                required property string code
                Tk.Flex.shrink: 0
                deviceName: name
                deviceModel: model
                pairingCode: code
                onAccepted: Devices.accept(id)
                onRejected: Devices.reject(id)
            }
        }

        Tk.Flex {
            direction: Tk.Flex.Row
            wrap: Tk.Flex.Wrap
            gap: Tk.Theme.space.sm
            align: Tk.Flex.Stretch

            Parts.LinkPanel {
                objectName: "phonePanel"
                title: qsTr("Phone")
                iconName: "smartphone"
                Tk.Flex.grow: 1
                Tk.Flex.basis: 380
                Tk.Flex.minWidth: 320
                summary: Devices.hasDevice ? (Devices.phoneReady ? qsTr("Ready") : qsTr("Not connected")) : ""

                Tk.Flex {
                    direction: Tk.Flex.Column
                    width: parent.width
                    align: Tk.Flex.Stretch
                    gap: Tk.Theme.space.xs
                    visible: Devices.hasDevice

                    Tk.Heading {
                        level: 3
                        text: Devices.device.name === undefined ? "" : Devices.device.name
                    }
                    Tk.Caption { text: Devices.device.model === undefined ? "" : Devices.device.model }
                    Tk.Divider { Tk.Flex.shrink: 0 }
                    Tk.KeyValue { key: qsTr("Connection"); value: Devices.device.transport ?? ""; mono: false }
                    Tk.KeyValue { key: qsTr("Battery"); value: Devices.device.battery || qsTr("Unknown"); mono: false }
                    Tk.KeyValue { key: qsTr("Network"); value: Devices.device.network || qsTr("Unknown"); mono: false }
                    Tk.KeyValue { key: qsTr("Last seen"); value: Devices.device.lastSeen ?? ""; mono: false }
                    Tk.KeyValue {
                        key: qsTr("Paired")
                        value: Devices.device.paired ? qsTr("Yes") : qsTr("No")
                        mono: false
                    }
                    Tk.Overline { title: qsTr("Capabilities") }
                    Tk.Flex {
                        wrap: Tk.Flex.Wrap
                        gap: Tk.Theme.space.xs
                        Repeater {
                            model: Devices.capabilities
                            delegate: Tk.Badge { required property string modelData; text: modelData }
                        }
                        Tk.Caption {
                            visible: Devices.capabilities.length === 0
                            text: qsTr("Not reported yet")
                        }
                    }
                    Tk.Flex {
                        gap: Tk.Theme.space.xs
                        paddingTop: Tk.Theme.space.sm
                        Tk.Button {
                            objectName: "unpairButton"
                            text: qsTr("Unpair")
                            iconName: "unlink"
                            variant: "danger"
                            small: true
                            enabled: Devices.device.paired === true
                            tooltip: qsTr("Forget this phone and close its connection")
                            onClicked: unpairDialog.open()
                        }
                    }
                }

                Item {
                    visible: !Devices.hasDevice
                    implicitWidth: 360
                    implicitHeight: 150
                    Tk.EmptyState {
                        anchors.fill: parent
                        maxTextWidth: 340
                        iconName: "smartphone"
                        title: qsTr("No phone")
                        text: qsTr("Install QindaLink on the phone, connect it with USB debugging on and open the app. "
                                   + "It appears here with a pairing code.")
                    }
                }
            }

            Parts.SetupSection {
                Tk.Flex.grow: 1
                Tk.Flex.basis: 380
                Tk.Flex.minWidth: 320
            }
        }
    }

    Tk.MessageDialog {
        id: unpairDialog
        objectName: "unpairDialog"
        title: qsTr("Unpair this phone?")
        variant: "warning"
        text: qsTr("%1 will be forgotten. Pairing again needs a new code on both screens.")
                  .arg(Devices.device.name ?? "")
        onAccepted: Devices.unpair(Devices.deviceId)
    }
}

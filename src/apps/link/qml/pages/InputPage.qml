// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.Link
import "../parts" as Parts

// Input & macros: an honest status page. The phone's touchpad and keyboard (using the phone to
// drive this desktop) are reported by capability and say "unavailable" until the input feature is
// part of this build; macros are edited in Settings, not here (ADR-0436).
Tk.Scroll {
    id: page
    objectName: "inputPage"
    padding: Tk.Theme.space.sm

    // AGENT-NOTE: flip when the desktop half of the input feature (capability `input`) lands on
    // main and the service can inject events; until then the rows below stay "unavailable".
    readonly property bool desktopInputBuilt: false
    readonly property bool phoneHasInput: Devices.capabilities.indexOf("input") >= 0

    Tk.Flex {
        width: page.viewportWidth - page.padding * 2
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Parts.ServiceNotice { Tk.Flex.shrink: 0 }

        Parts.LinkPanel {
            title: qsTr("Phone as input device")
            iconName: "mouse-pointer"
            Tk.Flex.shrink: 0
            Tk.Flex {
                width: parent.width
                direction: Tk.Flex.Column
                align: Tk.Flex.Stretch
                gap: Tk.Theme.space.xs
                Tk.KeyValue {
                    objectName: "touchpadStatus"
                    key: qsTr("Touchpad")
                    value: page.desktopInputBuilt && page.phoneHasInput ? qsTr("Available") : qsTr("Unavailable")
                    mono: false
                }
                Tk.KeyValue {
                    objectName: "keyboardStatus"
                    key: qsTr("Keyboard")
                    value: page.desktopInputBuilt && page.phoneHasInput ? qsTr("Available") : qsTr("Unavailable")
                    mono: false
                }
                Tk.Caption {
                    Tk.Flex.minWidth: 0
                    wrapMode: Text.WordWrap
                    text: !page.desktopInputBuilt
                          ? qsTr("Driving this desktop from the phone is not part of this build yet.")
                          : !Devices.hasDevice ? qsTr("Connect a phone to use it as a touchpad or keyboard.")
                          : !page.phoneHasInput ? qsTr("This phone's QindaLink app does not offer input.")
                          : qsTr("Open the QindaLink app on the phone to start.")
                }
            }
        }

        Parts.LinkPanel {
            title: qsTr("Macros")
            iconName: "square-pen"
            Tk.Flex.shrink: 0
            Tk.Flex {
                direction: Tk.Flex.Column
                gap: Tk.Theme.space.sm
                Tk.Caption {
                    Tk.Flex.minWidth: 0
                    wrapMode: Text.WordWrap
                    text: qsTr("Macros are edited in Settings, under Input. A macro can be started from the phone "
                               + "once it is assigned there.")
                }
                Tk.Flex {
                    Tk.Button {
                        objectName: "openMacros"
                        text: qsTr("Open Settings: Input: Macros")
                        iconName: "external-link"
                        small: true
                        onClicked: if (!Launcher.openMacroSettings()) console.warn("qindaqt-link: could not start Settings")
                    }
                }
            }
        }
    }
}

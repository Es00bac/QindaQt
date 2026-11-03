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

    // AGENT-CONTRACT: input is the phone's feature (capability `input` in Devices() caps). The desktop
    // grants it only while the session is unlocked and in front, and reports the gate (locked,
    // inactive, unverified, busy) to the phone as `input.state`, not to this app (protocol page,
    // "Remote input"). So this page shows what is knowable here -- whether the phone offers input
    // -- and says where the gate's reason appears, instead of claiming a state it cannot read.
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
                    value: page.phoneHasInput ? qsTr("Offered by the phone") : qsTr("Unavailable")
                    mono: false
                }
                Tk.KeyValue {
                    objectName: "keyboardStatus"
                    key: qsTr("Keyboard")
                    value: page.phoneHasInput ? qsTr("Offered by the phone") : qsTr("Unavailable")
                    mono: false
                }
                Tk.Caption {
                    Tk.Flex.minWidth: 0
                    wrapMode: Text.WordWrap
                    text: !Devices.hasDevice ? qsTr("Connect a phone to use it as a touchpad or keyboard.")
                          : !page.phoneHasInput ? qsTr("This phone's QindaLink app does not offer input.")
                          : qsTr("Start it from the QindaLink app on the phone. The desktop allows it only while this "
                                 + "session is unlocked and in front; the phone shows the reason when it is refused "
                                 + "(locked, inactive, or unverified).")
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

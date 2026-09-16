// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The mixing console (ADR-0173): input strips on the left, output buses on the
// right, and the routing matrix expressed as the assignment buttons along the
// bottom of each strip. This is the surface the VoiceMeeter Potato parity
// target describes; see docs/wiki/reference/voicemeeter-potato-parity.md for
// what is present and what is still absent.
ColumnLayout {
    id: root

    required property var audioSettings

    // Every console control follows one predicate, so an enabled control can
    // never be locally refused by the model's own admission check.
    // AGENT-GUARD: every model read carries a default. The Settings navigation
    // harness hosts this page against a duck-typed stub model and runs with
    // QT_FATAL_WARNINGS, so a property the stub does not implement aborts the
    // whole page rather than merely reading as undefined.
    readonly property bool available: audioSettings.consoleAvailable ?? false
    readonly property bool controlsEnabled:
        available && !(audioSettings.busy ?? false)
        && !(audioSettings.unavailable ?? false)

    objectName: "audioConsoleSection"
    spacing: Tokens.space["3"]
    visible: available

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Label {
            text: qsTr("Mixing console")
            Accessible.name: text
        }
        Item { Layout.fillWidth: true }
        Label {
            objectName: "audioConsoleSoloNotice"
            // Solo silences every other strip, which is a state a user can
            // leave switched on by accident and then not understand.
            visible: root.audioSettings.consoleSoloActive ?? false
            text: qsTr("Solo active — other inputs are silenced")
            font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })
            Accessible.name: text
        }
    }

    T.ScrollView {
        Layout.fillWidth: true
        contentHeight: consoleRack.implicitHeight
        clip: true

        RowLayout {
            id: consoleRack
            spacing: Tokens.space["4"]

            RowLayout {
                spacing: Tokens.space["2"]
                Repeater {
                    model: root.audioSettings.consoleStrips ?? []
                    AudioConsoleStrip {
                        required property var modelData
                        model: root.audioSettings
                        strip: modelData
                        buses: root.audioSettings.consoleBuses ?? []
                        soloActive: root.audioSettings.consoleSoloActive ?? false
                        enabledControls: root.controlsEnabled
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                implicitWidth: 1
                color: Tokens.outline.divider
            }

            RowLayout {
                spacing: Tokens.space["2"]
                Repeater {
                    model: root.audioSettings.consoleBuses ?? []
                    AudioConsoleBus {
                        required property var modelData
                        model: root.audioSettings
                        bus: modelData
                        enabledControls: root.controlsEnabled
                    }
                }
            }
        }
    }
}

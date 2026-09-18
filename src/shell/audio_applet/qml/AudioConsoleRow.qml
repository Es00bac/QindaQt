// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// One console strip in the tray (ADR-0181): label, a horizontal fader driven
// by position through the service's gain law, a segmented LED meter that reads
// the controller's level channel rather than the row, and a mute lamp.
RowLayout {
    id: row

    required property var controller
    required property var strip

    readonly property var reading: controller?.consoleLevels?.[strip?.id ?? ""]

    spacing: Tokens.space["2"]
    objectName: "audioConsoleRow_" + (strip?.id ?? "")
    visible: strip !== null
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Strip %1").arg(strip?.label ?? "")

    C.Label {
        Layout.preferredWidth: 96
        text: row.strip?.label ?? ""
        elide: Text.ElideRight
        opacity: (row.strip?.bound ?? false) ? 1.0 : 0.55
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        C.Slider {
            id: fader
            objectName: "audioConsoleFader_" + (row.strip?.id ?? "")
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            enabled: row.controller?.controlGranted === true
            accessibleName: qsTr("%1 fader").arg(row.strip?.label ?? "")

            // AGENT-CONTRACT (ADR-0191): a pressed control owns its value, so
            // the projected position rebinds only while the handle is free.
            // A plain `value: strip.faderPosition` binding would be destroyed
            // by the first imperative write and never follow truth again.
            Binding {
                target: fader
                property: "value"
                value: row.strip?.faderPosition ?? 0.0
                when: !fader.pressed
                restoreMode: Binding.RestoreNone
            }

            // One operation per drag, not one per pixel.
            onPressedChanged: if (!pressed && row.strip !== null
                                  && value !== row.strip.faderPosition)
                row.controller.requestStripFader(row.strip.id, value)
        }

        AudioLedMeter {
            objectName: "audioConsoleMeter_" + (row.strip?.id ?? "")
            Layout.fillWidth: true
            implicitHeight: 6
            reading: row.reading
        }
    }

    AudioAppletPad {
        objectName: "audioConsoleMute_" + (row.strip?.id ?? "")
        text: qsTr("M")
        checkable: true
        checked: row.strip?.muted ?? false
        destructive: true
        lampColor: Tokens.danger.default
        available: row.controller?.controlGranted === true
        onToggled: row.controller.requestStripMute(row.strip.id, checked)
        Accessible.name: qsTr("Mute %1").arg(row.strip?.label ?? "")
    }
}

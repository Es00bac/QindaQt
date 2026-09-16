// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// One console strip in the tray (ADR-0181): label, a horizontal fader driven
// by position through the service's gain law, mute, and a meter that reads
// the controller's level channel rather than the row.
RowLayout {
    id: row

    required property var controller
    required property var strip

    readonly property var reading: controller?.consoleLevels?.[strip.id]
    readonly property bool levelKnown: reading !== undefined && reading.known === true
    readonly property real meterFraction: levelKnown
        ? Math.max(0.0, Math.min(1.0, (reading.peakDb + 60.0) / 60.0))
        : 0.0

    spacing: Tokens.space["2"]
    objectName: "audioConsoleRow_" + strip.id
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Strip %1").arg(strip.label)

    C.Label {
        Layout.preferredWidth: 72
        text: row.strip.label
        elide: Text.ElideRight
        opacity: row.strip.bound ? 1.0 : 0.55
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0
        C.Slider {
            id: fader
            objectName: "audioConsoleFader_" + row.strip.id
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            value: row.strip.faderPosition
            enabled: row.controller?.controlGranted === true
            Accessible.name: qsTr("%1 fader").arg(row.strip.label)
            // One operation per drag, not one per pixel.
            onPressedChanged: if (!pressed && value !== row.strip.faderPosition)
                row.controller.requestStripFader(row.strip.id, value)
        }
        Rectangle {
            objectName: "audioConsoleMeter_" + row.strip.id
            Layout.fillWidth: true
            implicitHeight: 3
            radius: 1
            color: Tokens.bg.raised
            Rectangle {
                height: parent.height
                radius: parent.radius
                width: parent.width * row.meterFraction
                visible: row.levelKnown
                color: !row.levelKnown || row.reading.peakDb < -12.0
                    ? Tokens.status.success.foreground
                    : (row.reading.peakDb < -3.0 ? Tokens.status.warning.foreground
                                                 : Tokens.danger.default)
            }
        }
    }

    C.Button {
        objectName: "audioConsoleMute_" + row.strip.id
        text: qsTr("M")
        checkable: true
        checked: row.strip.muted
        enabled: row.controller?.controlGranted === true
        onToggled: row.controller.requestStripMute(row.strip.id, checked)
        Accessible.name: qsTr("Mute %1").arg(row.strip.label)
    }
}

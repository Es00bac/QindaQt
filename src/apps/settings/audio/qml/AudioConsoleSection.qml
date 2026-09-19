// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The mixing console (ADR-0173), condensed to desk density: every strip and
// bus is a narrow card (AudioConsoleStrip / AudioConsoleBus) and the cards
// flow left-to-right, wrapping onto as many rows as the window is wide. A
// wide window shows the whole desk at once; a narrow one stacks rows and the
// page scrolls VERTICALLY — AGENT-CONTRACT: there is deliberately no
// horizontal scroller anywhere in this surface; the layout must wrap instead.
// See docs/wiki/reference/voicemeeter-potato-parity.md for the feature target
// the cards implement.
ColumnLayout {
    id: root

    required property var audioSettings

    // Service availability governs the console's controls. A transport request
    // must not disable a held fader; its gesture coalesces until the wire is free.
    // AGENT-GUARD: every model read carries a default. The Settings navigation
    // harness hosts this page against a duck-typed stub model and runs with
    // QT_FATAL_WARNINGS, so a property the stub does not implement aborts the
    // whole page rather than merely reading as undefined.
    readonly property bool available: audioSettings.consoleAvailable ?? false
    readonly property bool controlsEnabled:
        available && !(audioSettings.unavailable ?? false)

    // At most one rack is open at a time: racks are full-width bands under
    // the cards, and two open bands would push every card row apart. The id
    // (not a delegate reference) is stored so a projection republish that
    // recreates the delegates cannot dangle the selection.
    property string openStripRackId: ""
    property string openBusRackId: ""

    objectName: "audioConsoleSection"
    Layout.fillWidth: true
    spacing: Tokens.space["2"]
    visible: available

    function toggleStripRack(stripId) {
        root.openStripRackId = root.openStripRackId === stripId ? "" : stripId
        root.openBusRackId = ""
    }
    function toggleBusRack(busId) {
        root.openBusRackId = root.openBusRackId === busId ? "" : busId
        root.openStripRackId = ""
    }
    function stripById(stripId) {
        const strips = root.audioSettings.consoleStrips ?? []
        for (const candidate of strips) {
            if (candidate.id === stripId)
                return candidate
        }
        return null
    }
    function busById(busId) {
        const buses = root.audioSettings.consoleBuses ?? []
        for (const candidate of buses) {
            if (candidate.id === busId)
                return candidate
        }
        return null
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        // The same header the device, virtual-device and stream sections use.
        // A bare Label here made the console — the surface a user operates
        // continuously — read as less important than the lists below it.
        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Mixing console")
            description: qsTr("Input strips, output buses, and their routing")
        }
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

    AudioConsolePresets {
        Layout.fillWidth: true
        model: root.audioSettings
        enabledControls: (root.audioSettings.ready ?? false) && !(root.audioSettings.busy ?? false)
    }

    // Input strips. Each card is fixed-width; the Flow is the row allocator.
    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Repeater {
            // ADR-0191 applies to console cards too: a snapshot updates a
            // held card's data, not the lifetime of its mouse grabber.
            model: (root.audioSettings.consoleStrips ?? []).length
            AudioConsoleStrip {
                required property int index
                readonly property var modelData:
                    root.audioSettings.consoleStrips?.[index] ?? ({
                        id: "", label: "", virtual: false, bound: false,
                        faderPosition: 0.0, sends: [], mono: false,
                        muted: false, soloed: false
                    })
                visible: modelData.id.length > 0
                model: root.audioSettings
                strip: modelData
                buses: root.audioSettings.consoleBuses ?? []
                soloActive: root.audioSettings.consoleSoloActive ?? false
                enabledControls: root.controlsEnabled
                rackVisible: root.openStripRackId === modelData.id
                onRackToggled: root.toggleStripRack(modelData.id)
            }
        }
    }

    // One full-width rack band, under the cards, for whichever strip or bus
    // asked for it. Loaders (not visible stacks) so closed racks cost nothing.
    Loader {
        id: stripRackLoader
        Layout.fillWidth: true
        active: root.openStripRackId !== "" && root.stripById(root.openStripRackId) !== null
        visible: active
        sourceComponent: AudioConsoleRack {
            model: root.audioSettings
            strip: root.stripById(root.openStripRackId) ?? ({})
            enabledControls: root.controlsEnabled
        }
    }
    Loader {
        id: busRackLoader
        Layout.fillWidth: true
        active: root.openBusRackId !== "" && root.busById(root.openBusRackId) !== null
        visible: active
        sourceComponent: AudioConsoleBusRack {
            model: root.audioSettings
            bus: root.busById(root.openBusRackId) ?? ({})
            enabledControls: root.controlsEnabled
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Tokens.outline.divider
    }

    // Output buses: the send destinations every strip's routing pads point at.
    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Repeater {
            model: (root.audioSettings.consoleBuses ?? []).length
            AudioConsoleBus {
                required property int index
                readonly property var modelData:
                    root.audioSettings.consoleBuses?.[index] ?? ({
                        id: "", label: "", virtual: false, bound: false,
                        faderPosition: 0.0, mono: false, muted: false
                    })
                visible: modelData.id.length > 0
                model: root.audioSettings
                bus: modelData
                enabledControls: root.controlsEnabled
                rackOpen: root.openBusRackId === modelData.id
                onRackToggled: root.toggleBusRack(modelData.id)
            }
        }
    }
}

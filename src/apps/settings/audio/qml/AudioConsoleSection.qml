// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaTK as Tk

// The mixing console (ADR-0173) at desk density, rebuilt on QindaTK
// (ADR-0227): every strip and bus is a narrow card (AudioConsoleStrip /
// AudioConsoleBus) and the cards flow left-to-right, wrapping onto as many
// rows as the window is wide. A wide window shows the whole desk at once; a
// narrow one stacks rows and the page scrolls VERTICALLY — AGENT-CONTRACT:
// there is deliberately no horizontal scroller anywhere in this surface; the
// layout must wrap instead. See docs/wiki/reference/voicemeeter-potato-parity.md
// for the feature target the cards implement.
Tk.Flex {
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
    direction: Tk.Flex.Column
    gap: Tk.Theme.space.sm
    visible: available
    // The page still stacks its sections with QtQuick.Layouts; only the
    // console's internals are QindaTK. Fill the width the column offers.
    Layout.fillWidth: true

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

    Tk.SectionHeader {
        title: qsTr("Mixing console")
        count: qsTr("%1 strips · %2 buses")
            .arg((root.audioSettings.consoleStrips ?? []).length)
            .arg((root.audioSettings.consoleBuses ?? []).length)
    }

    Tk.Notice {
        objectName: "audioConsoleSoloNotice"
        // Solo silences every other strip, which is a state a user can
        // leave switched on by accident and then not understand.
        visible: root.audioSettings.consoleSoloActive ?? false
        variant: "warning"
        text: qsTr("Solo active — other inputs are silenced")
        Accessible.name: text
    }

    AudioConsolePresets {
        model: root.audioSettings
        enabledControls: (root.audioSettings.ready ?? false) && !(root.audioSettings.busy ?? false)
    }

    // Input strips. Each card is fixed-width; the wrapping Flex is the row
    // allocator.
    Tk.Flex {
        wrap: Tk.Flex.Wrap
        gap: Tk.Theme.space.sm

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
                        pan: 0.0, muted: false, soloed: false
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
        active: root.openBusRackId !== "" && root.busById(root.openBusRackId) !== null
        visible: active
        sourceComponent: AudioConsoleBusRack {
            model: root.audioSettings
            bus: root.busById(root.openBusRackId) ?? ({})
            enabledControls: root.controlsEnabled
        }
    }

    Tk.Divider {}

    // Output buses: the send destinations every strip's routing lamps point at.
    Tk.Flex {
        wrap: Tk.Flex.Wrap
        gap: Tk.Theme.space.sm

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

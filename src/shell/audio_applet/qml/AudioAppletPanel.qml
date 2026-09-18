// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The audio panel body: two default-device bands, the application streams,
// the console strips, and one status line. Laid out on the 4 px grid with
// hairline separators, so the popup reads as a piece of desk equipment rather
// than a menu.
//
// AGENT-CONTRACT: `store` is the AudioApplet root item, which outlives this
// panel — the Popup destroys its contents on close, so every piece of state a
// user would expect to survive closing the panel (which bands are collapsed,
// which device each band rides) is read and written there, never here.
ColumnLayout {
    id: panel

    required property var controller
    required property var store

    readonly property bool showLists:
        controller?.phaseText === "ready"
            || controller?.phaseText === "degraded"

    // The one status line, assembled from every condition that used to own a
    // card of its own. Empty means nothing is wrong and nothing is shown.
    readonly property string statusText: {
        if (!controller)
            return qsTr("Audio controls are not connected")
        if (controller.feedbackPresent)
            return controller.feedback
        if (controller.phaseText === "loading")
            return qsTr("Audio device information is loading…")
        if (controller.phaseText === "unavailable")
            return qsTr("Audio is unavailable. %1").arg(controller.phaseReasonText ?? "")
        if (controller.phaseText === "degraded")
            return qsTr("Audio information is limited. %1").arg(controller.phaseReasonText ?? "")
        return ""
    }
    readonly property bool statusIsError:
        (controller?.feedbackPresent ?? false)
            || controller?.phaseText === "unavailable"

    spacing: Tokens.space["2"]

    AudioAppletBand {
        objectName: "audioOutputBand"
        Layout.fillWidth: true
        title: qsTr("Output")
        expanded: panel.store.outputExpanded
        visible: panel.showLists
        onToggled: panel.store.outputExpanded = !panel.store.outputExpanded
    }

    AudioDeviceRow {
        objectName: "audioOutputRow"
        Layout.fillWidth: true
        visible: panel.showLists && panel.store.outputExpanded
        controller: panel.controller
        outputs: true
        selectedSerial: panel.store.outputSelectedSerial
        onDevicePicked: serial => panel.store.outputSelectedSerial = serial
    }

    AudioAppletBand {
        objectName: "audioInputBand"
        Layout.fillWidth: true
        title: qsTr("Input")
        expanded: panel.store.inputExpanded
        visible: panel.showLists
        onToggled: panel.store.inputExpanded = !panel.store.inputExpanded
    }

    AudioDeviceRow {
        objectName: "audioInputRow"
        Layout.fillWidth: true
        visible: panel.showLists && panel.store.inputExpanded
        controller: panel.controller
        outputs: false
        selectedSerial: panel.store.inputSelectedSerial
        onDevicePicked: serial => panel.store.inputSelectedSerial = serial
    }

    AudioAppletBand {
        objectName: "audioAppsBand"
        Layout.fillWidth: true
        title: qsTr("Apps")
        expanded: panel.store.appsExpanded
        visible: panel.showLists && (panel.controller?.streamRows?.length ?? 0) > 0
        onToggled: panel.store.appsExpanded = !panel.store.appsExpanded
    }

    // AGENT-GUARD (ADR-0191): the model is the row *count*, not the row list.
    // A Repeater handed a QVariantList regenerates every delegate whenever that
    // list is reassigned, and the controller reassigns it on every
    // reprojection -- including the one its own dispatch triggers. That
    // destroyed the control the user was holding, so a pointer drag lost its
    // grab and a keyboard step lost its focus after exactly one move. Binding
    // the row by index keeps the item alive and updates its values in place.
    Repeater {
        objectName: "audioStreamRows"
        model: panel.showLists && panel.store.appsExpanded
               ? (panel.controller?.streamRows?.length ?? 0) : 0

        delegate: AudioStreamRow {
            required property int index

            Layout.fillWidth: true
            row: panel.controller.streamRows[index] ?? null
            controller: panel.controller
        }
    }

    AudioAppletBand {
        objectName: "audioConsoleBand"
        Layout.fillWidth: true
        title: qsTr("Console")
        expanded: panel.store.consoleExpanded
        visible: panel.showLists && (panel.controller?.consoleRows?.length ?? 0) > 0
        onToggled: panel.store.consoleExpanded = !panel.store.consoleExpanded
    }

    // The console (ADR-0181): the strips' faders, mutes and meters, so a level
    // can be ridden from the tray without opening Settings.
    Repeater {
        objectName: "audioConsoleRows"
        model: panel.showLists && panel.store.consoleExpanded
               ? (panel.controller?.consoleRows?.length ?? 0) : 0
        delegate: AudioConsoleRow {
            required property int index
            Layout.fillWidth: true
            controller: panel.controller
            strip: panel.controller.consoleRows[index] ?? null
        }
    }

    AudioAppletFooter {
        objectName: "audioAppletFooter"
        Layout.fillWidth: true
        Layout.topMargin: Tokens.space["1"]
        controller: panel.controller
        desktopControls: panel.store.desktopControls
        statusText: panel.statusText
        statusIsError: panel.statusIsError
        showLists: panel.showLists
        onDismissRequested: panel.controller.clearFeedback()
    }
}

// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The Audio page's heading and service status (ADR-0288): a healthy service
// is one word beside the heading, and the full state card, the in-flight
// change and any error appear only when there is something to say.
ColumnLayout {
    id: root

    required property var audioSettings

    spacing: Tokens.space["2"]

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Label {
            objectName: "audioPageHeading"
            Layout.fillWidth: true
            text: qsTr("Audio")
            font.pointSize: Tokens.type.subtitle
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        // Compact (ADR-0288): a healthy service is one word beside the
        // heading; the full card below appears only when there is
        // something to explain.
        Label {
            objectName: "audioServiceReady"
            visible: root.audioSettings.ready
            text: qsTr("Ready")
            muted: true
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Audio service ready")
        }
    }

    StateCard {
        id: serviceState
        objectName: "audioServiceState"
        visible: !root.audioSettings.ready
        Layout.fillWidth: true
        status: root.audioSettings.loading ? StateCard.Busy
                : root.audioSettings.ready ? StateCard.Success
                : root.audioSettings.stale || root.audioSettings.degraded
                  ? StateCard.Warning
                : StateCard.Error
        title: root.audioSettings.stale
               ? qsTr("Stale audio information")
               : root.audioSettings.degraded
                 ? qsTr("Limited audio information")
               : root.audioSettings.ready ? qsTr("Audio service ready")
               : qsTr("Audio service unavailable")
        message: root.audioSettings.statusText
    }

    Label {
        objectName: "audioOperationStatus"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.audioSettings.operationStatusText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Label {
        objectName: "audioError"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.audioSettings.errorText
        color: Tokens.fg.default
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}

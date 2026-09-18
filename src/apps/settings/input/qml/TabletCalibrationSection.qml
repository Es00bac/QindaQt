// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Calibration: say whether the tablet is calibrated, start the wizard, or
// put it back the way it shipped.
ColumnLayout {
    id: root

    required property var selection
    signal calibrationRequested()

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Calibration")
        description: qsTr("Line the pen tip up with what is under it on a pen display")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Label {
            objectName: "tabletCalibrationState"
            Layout.fillWidth: true
            muted: true
            text: root.selection !== null && root.selection.calibrated
                  ? qsTr("This tablet is calibrated.")
                  : qsTr("This tablet uses the calibration it shipped with.")
        }

        Button {
            objectName: "tabletCalibrationStart"
            text: qsTr("Calibrate…")
            // AGENT-CONTRACT: calibration happens on the screen the pen is
            // mapped to. A tablet that follows the active screen has no
            // fixed surface to calibrate against, so the button is
            // unavailable rather than opening a wizard that would measure
            // the wrong thing.
            available: root.selection !== null
                       && root.selection.mapMode === "output"
                       && root.selection.outputName.length > 0
            accessibleDescription: available
                ? qsTr("Touch four crosshairs with the pen on %1 to line it up")
                    .arg(root.selection.outputName)
                : qsTr("Choose a screen under Map to before calibrating")
            onClicked: {
                // AGENT-CONTRACT: Measuring through an existing matrix would
                // compose two corrections and drift further every pass, so
                // the wizard always starts from the device's own calibration.
                root.selection.resetCalibration()
                root.calibrationRequested()
            }
        }

        Button {
            objectName: "tabletCalibrationReset"
            text: qsTr("Reset")
            emphasized: false
            available: root.selection !== null && root.selection.calibrated
            accessibleDescription: qsTr("Restore the calibration this tablet shipped with")
            onClicked: root.selection.resetCalibration()
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// What can be done to the selected process. Kept beside the table rather
// than behind a dialog so the consequence of a signal is visible in the same
// glance as the decision to send one.
Tk.Flex {
    id: actions

    property int nice: 0
    property string state: ""

    signal requested(string action, int value)

    gap: Tk.Theme.space.sm
    align: Tk.Flex.Center

    Tk.Button {
        text: qsTr("Terminate")
        small: true
        iconName: "circle-x"
        tooltip: qsTr("Ask the process to exit (SIGTERM)")
        onClicked: actions.requested("terminate", 0)
    }
    Tk.Button {
        text: qsTr("Kill")
        small: true
        variant: "danger"
        iconName: "skull"
        tooltip: qsTr("Stop the process immediately (SIGKILL). Unsaved work is lost.")
        onClicked: actions.requested("kill", 0)
    }
    Tk.Button {
        text: actions.state === "T" ? qsTr("Continue") : qsTr("Pause")
        small: true
        iconName: actions.state === "T" ? "play" : "pause"
        tooltip: qsTr("Suspend or resume the process")
        onClicked: actions.requested(actions.state === "T" ? "continue" : "stop", 0)
    }
    Tk.Spacer {}
    Tk.Caption { text: qsTr("Nice") }
    Tk.NumberField {
        objectName: "niceField"
        small: true
        implicitWidth: 64
        from: -20
        to: 19
        stepSize: 1
        value: actions.nice
        tooltip: qsTr("Scheduling priority: lower runs sooner. Below zero needs privileges.")
        onValueModified: actions.requested("nice", value)
    }
}

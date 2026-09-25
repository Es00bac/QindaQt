// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// One job's visible state (ADR-0275 section 5): progress with a plain stage
// line and Cancel while it runs; afterwards ONE plain sentence, an optional
// note, and "Copy details" for whoever is helping. Values in, signals out.
Tk.Flex {
    id: job

    property bool busy: false
    property real progress: 0
    property string stageText: ""
    property string resultMessage: ""
    property string resultNote: ""
    property bool succeeded: false
    property string details: ""

    signal cancelRequested()
    signal copyDetailsRequested(string details)

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.sm
    visible: busy || resultMessage.length > 0

    Tk.Card {
        objectName: "jobProgressCard"
        visible: job.busy
        Tk.Flex.alignSelf: Tk.Flex.Stretch

        Tk.Flex {
            direction: Tk.Flex.Row
            align: Tk.Flex.Center
            gap: Tk.Theme.space.md
            width: parent.width

            Tk.Flex {
                direction: Tk.Flex.Column
                gap: Tk.Theme.space.xs
                Tk.Flex.grow: 1
                Tk.Flex.basis: 0
                Tk.Label {
                    objectName: "jobStageText"
                    text: job.stageText
                    elide: Text.ElideRight
                    Tk.Flex.alignSelf: Tk.Flex.Stretch
                }
                Tk.ProgressBar {
                    objectName: "jobProgressBar"
                    from: 0
                    to: 1
                    value: job.progress
                    tooltip: qsTr("%1 %").arg(Math.round(job.progress * 100))
                    Tk.Flex.alignSelf: Tk.Flex.Stretch
                }
            }
            Tk.Button {
                objectName: "jobCancelButton"
                text: qsTr("Cancel")
                iconName: "x"
                tooltip: qsTr("Stop, and remove what was started")
                onClicked: job.cancelRequested()
            }
        }
    }

    Tk.Notice {
        objectName: "jobResultNotice"
        visible: !job.busy && job.resultMessage.length > 0
        variant: job.succeeded ? "success" : "danger"
        title: job.resultMessage
        text: job.resultNote
        Tk.Flex.alignSelf: Tk.Flex.Stretch

        actions: [
            Tk.Button {
                visible: job.details.length > 0
                text: qsTr("Copy details")
                iconName: "copy"
                small: true
                tooltip: qsTr("Copy what happened, for someone helping you")
                onClicked: job.copyDetailsRequested(job.details)
            }
        ]
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor

// What is known about the selected process, and the things that can be done
// to it. Kept beside the table rather than in a dialog so the consequence of
// a signal is visible in the same glance as the decision to send one.
Tk.Box {
    id: details

    property var record: ({})
    readonly property bool valid: details.record.pid !== undefined

    signal actionRequested(string action, int value)

    color: Tk.Theme.color.panelAlt
    radius: Tk.Theme.radius.sm
    padding: Tk.Theme.space.sm
    implicitHeight: body.implicitHeight + padding * 2

    Tk.Flex {
        id: body
        width: parent.width
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.xs

        Tk.Flex {
            width: parent.width
            align: Tk.Flex.Center
            gap: Tk.Theme.space.sm

            Tk.Label {
                text: details.valid && details.record.name !== undefined
                      ? details.record.name : qsTr("No process selected")
                font.weight: Font.DemiBold
                Tk.Flex.shrink: 1
                Tk.Flex.minWidth: 0
            }
            Tk.Badge {
                visible: details.valid
                text: details.valid ? "PID " + details.record.pid : ""
            }
            Tk.Badge {
                visible: details.valid
                text: details.record.user !== undefined ? details.record.user : ""
                variant: "muted"
            }
        }

        Tk.Mono {
            width: parent.width
            visible: details.valid
            text: details.record.command !== undefined ? details.record.command : ""
            color: Tk.Theme.color.textMuted
            elide: Text.ElideMiddle
        }

        Tk.Flex {
            width: parent.width
            visible: details.valid
            gap: Tk.Theme.space.lg
            wrap: Tk.Flex.Wrap

            Tk.KeyValue { key: qsTr("CPU"); value: Facade.percent(details.record.cpu) }
            Tk.KeyValue { key: qsTr("Memory"); value: Facade.bytes(details.record.memory) }
            Tk.KeyValue { key: qsTr("Read"); value: Facade.rate(details.record.readRate) }
            Tk.KeyValue { key: qsTr("Write"); value: Facade.rate(details.record.writeRate) }
            Tk.KeyValue {
                key: qsTr("Threads")
                value: details.valid ? details.record.threads + "" : ""
            }
            Tk.KeyValue {
                key: qsTr("Nice")
                value: details.valid ? details.record.nice + "" : ""
            }
            Tk.KeyValue {
                key: qsTr("State")
                value: details.record.state !== undefined ? details.record.state : ""
            }
        }

        ProcessActions {
            width: parent.width
            visible: details.valid
            nice: details.valid ? details.record.nice : 0
            state: details.record.state !== undefined ? details.record.state : ""
            onRequested: function(action, value) { details.actionRequested(action, value) }
        }
    }
}

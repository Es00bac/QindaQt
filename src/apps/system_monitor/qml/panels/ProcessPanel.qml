// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Processes: filter, sort, tree, and act. The table holds no opinion about
// ordering -- it asks, and ProcessTableModel decides, because only the model
// knows that "cpu" is a number and "command" is text.
Parts.MonitorPanel {
    id: panel

    property alias filterText: filter.text
    readonly property var selected: table.currentIndex >= 0
                                    ? Processes.get(table.currentIndex) : ({})

    function act(action, value) {
        if (panel.selected.pid === undefined) {
            return
        }
        const message = Monitor.processAction(panel.selected.pid,
                                              panel.selected.startTicks, action, value)
        if (message.length > 0) {
            panel.actionFailed(message)
        }
    }

    signal actionFailed(string message)

    panelId: "processes"
    title: qsTr("Processes")
    iconName: "list"
    summary: Processes.count === Processes.totalCount
             ? Processes.totalCount + ""
             : Processes.count + " / " + Processes.totalCount

    controls: Tk.Flex {
        align: Tk.Flex.Center
        gap: Tk.Theme.space.xs

        Tk.SearchField {
            id: filter
            objectName: "processFilter"
            small: true
            implicitWidth: 150
            placeholderText: qsTr("Filter")
            tooltip: qsTr("Match on name, command, user or PID")
            onTextChanged: Processes.filterText = text
        }
        Tk.IconButton {
            objectName: "treeToggle"
            iconName: "list-tree"
            small: true
            checkable: true
            checked: Processes.treeMode
            tooltip: qsTr("Show processes as a tree")
            onToggled: Processes.treeMode = checked
        }
        Tk.IconButton {
            objectName: "ownToggle"
            iconName: "user"
            small: true
            checkable: true
            checked: Processes.ownProcessesOnly
            tooltip: qsTr("Only my processes")
            onToggled: Processes.ownProcessesOnly = checked
        }
    }

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Tk.DataTable {
            id: table
            objectName: "processTable"
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            model: Processes
            alternatingRows: true
            sortKey: Processes.sortKey
            sortOrder: Processes.sortOrder
            emptyText: qsTr("No process matches this filter")

            // AGENT-GUARD: the table asks, the model sorts. Assigning to the
            // model is what moves the rows; writing table.sortKey alone would
            // only move the header caret and leave the order untouched.
            onSortRequested: function(key, order) {
                Processes.sortKey = key
                Processes.sortOrder = order
            }
            onRowRightClicked: function(index, point) {
                table.currentIndex = index
                rowMenu.popup()
            }

            columns: [
                Tk.TableColumn {
                    key: "pid"; title: qsTr("PID"); width: 68; minWidth: 44
                    align: Qt.AlignRight; mono: true; descendingFirst: true
                },
                Tk.TableColumn {
                    key: "name"; title: qsTr("Program"); width: 150; flex: 1; minWidth: 90
                    delegate: Component {
                        Tk.Flex {
                            id: nameCell
                            // The delegate reads its data from the cell host
                            // (its parent); children read it from here,
                            // because their own parent is this Flex.
                            readonly property var record: nameCell.parent !== null
                                                          ? nameCell.parent.row : undefined
                            readonly property var text: nameCell.parent !== null
                                                        ? nameCell.parent.value : undefined

                            anchors.fill: parent
                            align: Tk.Flex.Center
                            gap: 2

                            // The tree's indent lives in the cell, so folding
                            // does not disturb any other column's alignment.
                            // AGENT-GUARD: `size`, not implicitWidth. A
                            // Tk.Spacer left at its default size of -1 sets
                            // grow:1 and swallows the whole cell, which
                            // right-aligned every process name in the table.
                            Tk.Spacer {
                                size: nameCell.record !== undefined
                                      ? nameCell.record.depth * 10 : 0
                            }
                            Tk.IconButton {
                                visible: nameCell.record !== undefined
                                         && nameCell.record.hasChildren
                                iconName: nameCell.record !== undefined
                                          && nameCell.record.collapsed
                                          ? "chevron-right" : "chevron-down"
                                small: true
                                ghost: true
                                implicitWidth: 14
                                tooltip: qsTr("Fold this subtree")
                                onClicked: Processes.toggleCollapsed(
                                               Processes.indexOfProcess(nameCell.record.pid,
                                                                        nameCell.record.startTicks))
                            }
                            Tk.Label {
                                text: nameCell.text === undefined ? "" : nameCell.text
                                Tk.Flex.shrink: 1
                                Tk.Flex.minWidth: 0
                            }
                        }
                    }
                },
                Tk.TableColumn {
                    key: "user"; title: qsTr("User"); width: 76; muted: true
                    visible: panel.width > 460
                },
                Tk.TableColumn {
                    key: "threads"; title: qsTr("Thr"); width: 42
                    align: Qt.AlignRight; mono: true; muted: true; descendingFirst: true
                    // AGENT-GUARD: the fixed columns have to fit, or the last
                    // one (CPU%, the column people sort by) is what falls off
                    // the right edge. Thread count and the I/O rates are the
                    // ones that answer a narrower question, so they are the
                    // ones that go when the panel is not wide enough.
                    visible: panel.width > 560
                },
                Tk.TableColumn {
                    key: "memory"; title: qsTr("Memory"); width: 80
                    align: Qt.AlignRight; mono: true; descendingFirst: true
                    formatter: function(v) { return Facade.bytes(v) }
                },
                Tk.TableColumn {
                    key: "readRate"; title: qsTr("Read"); width: 76
                    align: Qt.AlignRight; mono: true; muted: true; descendingFirst: true
                    visible: panel.width > 760
                    formatter: function(v) { return Facade.rate(v) }
                },
                Tk.TableColumn {
                    key: "writeRate"; title: qsTr("Write"); width: 76
                    align: Qt.AlignRight; mono: true; muted: true; descendingFirst: true
                    visible: panel.width > 760
                    formatter: function(v) { return Facade.rate(v) }
                },
                Tk.TableColumn {
                    key: "cpuHistory"; title: ""; width: 54
                    sortable: false; resizable: false
                    visible: panel.width > 620
                    delegate: Component {
                        Tk.Sparkline {
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 54
                            implicitHeight: 12
                            ramp: Tk.Theme.ramp.load
                            maxValue: 100
                            values: parent.value === undefined ? [] : parent.value
                        }
                    }
                },
                Tk.TableColumn {
                    key: "cpu"; title: qsTr("CPU%"); width: 56; minWidth: 48
                    align: Qt.AlignRight; mono: true; descendingFirst: true
                    ramp: Tk.Theme.ramp.load; rampFrom: 0; rampTo: 100
                    formatter: function(v) { return Facade.number(v, 1) }
                }
            ]
        }

        Parts.ProcessDetails {
            record: panel.selected
            onActionRequested: function(action, value) { panel.act(action, value) }
        }
    }

    Tk.Menu {
        id: rowMenu
        Tk.MenuItem {
            text: qsTr("Terminate")
            onTriggered: panel.act("terminate", 0)
        }
        Tk.MenuItem {
            text: qsTr("Kill")
            onTriggered: panel.act("kill", 0)
        }
        Tk.MenuSeparator {}
        Tk.MenuItem {
            text: qsTr("Pause")
            onTriggered: panel.act("stop", 0)
        }
        Tk.MenuItem {
            text: qsTr("Continue")
            onTriggered: panel.act("continue", 0)
        }
    }
}

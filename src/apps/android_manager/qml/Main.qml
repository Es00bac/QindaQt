// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QindaTK as Tk

Tk.AppWindow {
    id: window
    objectName: "androidApplicationsWindow"
    required property var manager
    width: 780
    height: 620
    minimumWidth: 420
    minimumHeight: 340
    title: qsTr("Android Applications")

    function stateLabel(state) {
        switch (state) {
        case "needsSetup": return qsTr("Setup required")
        case "notMaterialized": return qsTr("Installation required")
        case "ready": return qsTr("Ready")
        case "starting": return qsTr("Starting…")
        case "running": return qsTr("Running")
        case "stopping": return qsTr("Stopping…")
        case "needsRepair": return qsTr("Needs attention")
        default: return qsTr("Unavailable")
        }
    }

    toolBars: Tk.ToolBar {
        wrap: true
        Tk.SearchField {
            objectName: "androidSearch"
            implicitWidth: 260
            maximumLength: window.manager.queryLimit
            placeholderText: qsTr("Search Android applications")
            Accessible.name: qsTr("Search Android applications")
            onSearched: text => window.manager.query = text
        }
        Tk.Spacer {}
        Tk.Button {
            objectName: "androidRefresh"
            text: qsTr("Refresh")
            iconName: "refresh-cw"
            enabled: window.manager.canRefresh
            onClicked: window.manager.refresh()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tk.Theme.space.lg
        spacing: Tk.Theme.space.md
        Tk.Card {
            Layout.fillWidth: true
            implicitHeight: summary.implicitHeight + padding * 2
            ColumnLayout {
                id: summary
                anchors.fill: parent
                spacing: Tk.Theme.space.sm
                Tk.Heading { text: qsTr("Android applications"); Layout.fillWidth: true }
                Tk.Caption {
                    objectName: "androidRuntimeState"
                    text: window.stateLabel(window.manager.runtimeState)
                    Accessible.name: qsTr("Android status: %1").arg(text)
                }
                Tk.Label {
                    objectName: "androidRuntimeMessage"
                    text: window.manager.runtimeMessage
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        Tk.Label {
            objectName: "androidError"
            visible: text.length > 0
            text: window.manager.lastError
            color: Tk.Theme.color.danger
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Accessible.role: Accessible.AlertMessage
        }
        ListView {
            id: applications
            objectName: "androidApplicationList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Tk.Theme.space.sm
            model: window.manager.applications
            delegate: Tk.Card {
                id: row
                required property var modelData
                width: applications.width
                implicitHeight: rowContents.implicitHeight + padding * 2
                ColumnLayout {
                    id: rowContents
                    anchors.fill: parent
                    spacing: Tk.Theme.space.sm
                    RowLayout {
                        Layout.fillWidth: true
                        Tk.Icon { name: "app-window"; size: Tk.Theme.size.iconLg; color: Tk.Theme.color.textMuted }
                        Tk.Label {
                            text: row.modelData.name
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            font.weight: Font.DemiBold
                        }
                        // Registered catalog origin is an identity tint only;
                        // it never replaces focus, error or runtime status.
                        Tk.Badge {
                            objectName: "androidOrigin-" + row.modelData.entryId
                            text: qsTr("Android")
                            variant: "success"
                            tooltip: qsTr("Android application")
                        }
                        Tk.Button {
                            objectName: "androidLaunch-" + row.modelData.entryId
                            text: row.modelData.launchPending ? qsTr("Launching…") : qsTr("Launch")
                            enabled: row.modelData.launchEnabled
                            Accessible.name: qsTr("Launch %1").arg(row.modelData.name)
                            onClicked: window.manager.launch(row.modelData.entryId)
                        }
                    }
                    Tk.Caption {
                        text: row.modelData.launchPending ? qsTr("Waiting for launch confirmation…")
                            : window.stateLabel(row.modelData.state)
                        Accessible.name: qsTr("%1: %2").arg(row.modelData.name).arg(text)
                    }
                    Tk.Caption {
                        visible: row.modelData.hasWindowCount
                        text: qsTr("%n window(s)", "", row.modelData.windowCount)
                    }
                    Tk.Label {
                        visible: text.length > 0
                        text: row.modelData.message
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
            Tk.EmptyState {
                anchors.fill: parent
                visible: applications.count === 0
                iconName: "app-window"
                title: window.manager.query.length > 0 ? qsTr("No matching applications") : qsTr("No Android applications")
                text: window.manager.query.length > 0
                    ? qsTr("Try another application name or keyword.")
                    : qsTr("Registered Android applications will appear here when Android support is available.")
            }
        }
    }
}

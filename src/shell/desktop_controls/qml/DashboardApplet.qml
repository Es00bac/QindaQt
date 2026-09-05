// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Windows-modern "widgets" board: date and time, the system status lanes, the
// workspace strip, and pinned/recent applications. It receives the composite
// desktop-controls facade and reads only its sub-facades; every action still
// goes through the facade that owns it.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property var statusAccess: ready ? access.systemStatus : null
    readonly property var workspaceAccess: ready ? access.workspaces : null
    readonly property var launcherAccess: ready ? access.launcher : null
    property date now: new Date()
    property var applicationRows: []

    objectName: "dashboardApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    function refreshApplications() {
        const rows = []
        if (root.launcherAccess === null || root.launcherAccess === undefined) {
            root.applicationRows = rows
            return
        }
        const sections = root.launcherAccess.sectionsForQuery("")
        for (let s = 0; s < sections.length; ++s) {
            const kind = String(sections[s].kind)
            if (kind !== "pinned" && kind !== "recent")
                continue
            const items = sections[s].items
            for (let i = 0; i < items.length && rows.length < 12; ++i)
                rows.push({ "entryId": items[i].entryId, "displayText": items[i].displayText,
                            "iconName": items[i].iconName,
                            "detail": kind === "pinned" ? qsTr("Pinned") : qsTr("Recent") })
        }
        root.applicationRows = rows
    }

    Timer {
        interval: 1000
        repeat: true
        running: board.opened
        onTriggered: root.now = new Date()
    }

    Connections {
        target: root.launcherAccess
        function onStateChanged() { root.refreshApplications() }
    }

    DesktopControlButton {
        id: button
        objectName: "dashboardButton"
        anchors.fill: parent
        iconName: "dashboard-show"
        fallbackText: qsTr("Dashboard")
        vertical: root.vertical
        available: root.ready
        active: board.opened
        Accessible.name: qsTr("Dashboard")
        accessibleDescription: available
                               ? qsTr("Opens the date, status, workspace, and application board")
                               : qsTr("The dashboard is unavailable")
        onTriggered: {
            if (!available)
                return
            root.now = new Date()
            root.refreshApplications()
            board.open()
        }
    }

    ControlPopupFrame {
        id: board
        objectName: "dashboardPopup"
        width: 360
        heading: Qt.formatTime(root.now, Locale.ShortFormat)
        initialFocusItem: workspaces
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)

        C.Label {
            objectName: "dashboardDate"
            Layout.fillWidth: true
            text: Qt.formatDate(root.now, Locale.LongFormat)
            muted: true
        }

        C.SectionHeader {
            Layout.fillWidth: true
            visible: root.statusAccess !== null
            title: qsTr("Status")
        }

        Repeater {
            model: root.statusAccess !== null ? root.statusAccess.laneRows : []

            C.Label {
                required property var modelData
                objectName: "dashboardStatusLane"
                Layout.fillWidth: true
                text: Boolean(modelData.available) ? String(modelData.summary)
                      : qsTr("%1 %2").arg(String(modelData.label)).arg(String(modelData.phase))
                Accessible.name: String(modelData.accessibleName)
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        C.SectionHeader {
            Layout.fillWidth: true
            visible: root.workspaceAccess !== null
            title: qsTr("Workspaces")
        }

        WorkspaceStrip {
            id: workspaces
            objectName: "dashboardWorkspaces"
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            visible: root.workspaceAccess !== null
            access: root.workspaceAccess
            tiles: true
            tileExtent: 48
        }

        C.SectionHeader {
            Layout.fillWidth: true
            visible: root.applicationRows.length > 0
            title: qsTr("Applications")
        }

        Repeater {
            model: root.applicationRows

            MenuRow {
                required property var modelData
                objectName: "dashboardApplicationRow"
                Layout.fillWidth: true
                iconName: String(modelData.iconName)
                text: String(modelData.displayText)
                detail: String(modelData.detail)
                enabled: root.launcherAccess !== null && Boolean(root.launcherAccess.launchGranted)
                onActivated: if (root.launcherAccess.activate(String(modelData.entryId))) board.close()
            }
        }
    }
}

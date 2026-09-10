// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Presents the non-Ready navigation states (empty folder, permission denied,
// missing, not a folder, or an unclassified error) as one accessible banner
// instead of an empty list that could otherwise look identical to a
// still-loading or hung view.
Item {
    id: root

    property string statusKey: "empty"
    property string statusMessage: ""

    signal retryRequested()

    readonly property var presentation: ({
        "empty": {
            title: qsTr("This folder is empty"),
            showRetry: false
        },
        "permission-denied": {
            title: qsTr("Permission denied"),
            showRetry: true
        },
        "missing": {
            title: qsTr("Folder not found"),
            showRetry: true
        },
        "not-a-directory": {
            title: qsTr("Not a folder"),
            showRetry: true
        },
        "error": {
            title: qsTr("Couldn't read this folder"),
            showRetry: true
        }
    })
    readonly property var current: presentation[statusKey] !== undefined
        ? presentation[statusKey] : presentation["error"]

    Column {
        anchors.centerIn: parent
        width: Math.min(420, root.width - 32)
        spacing: 12
        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.statusKey === "empty" && root.height >= 280
            width: 120
            height: 120
            source: "qrc:/qindaqt/file-manager/empty-folder.png"
            fillMode: Image.PreserveAspectFit
            Accessible.ignored: true
        }
        StatusBanner {
            objectName: "navigationStateCard"
            width: parent.width
            title: root.current.title
            message: root.statusKey === "empty" ? qsTr("A little room for something new.") : root.statusMessage
            actionText: root.current.showRetry ? qsTr("Retry") : ""
            onActionTriggered: root.retryRequested()
        }
    }
}

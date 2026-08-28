// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

// Presents the non-Ready navigation states (empty folder, permission denied,
// missing, not a folder, or an unclassified error) as one accessible
// QindaQt.Controls StateCard instead of an empty list that could otherwise
// look identical to a still-loading or hung view.
Item {
    id: root

    property string statusKey: "empty"
    property string statusMessage: ""

    signal retryRequested()

    readonly property var presentation: ({
        "empty": {
            status: Qinda.StateCard.Information,
            title: qsTr("This folder is empty"),
            showRetry: false
        },
        "permission-denied": {
            status: Qinda.StateCard.Warning,
            title: qsTr("Permission denied"),
            showRetry: true
        },
        "missing": {
            status: Qinda.StateCard.Error,
            title: qsTr("Folder not found"),
            showRetry: true
        },
        "not-a-directory": {
            status: Qinda.StateCard.Error,
            title: qsTr("Not a folder"),
            showRetry: true
        },
        "error": {
            status: Qinda.StateCard.Error,
            title: qsTr("Couldn't read this folder"),
            showRetry: true
        }
    })
    readonly property var current: presentation[statusKey] !== undefined
        ? presentation[statusKey] : presentation["error"]

    Qinda.StateCard {
        objectName: "navigationStateCard"
        anchors.centerIn: parent
        width: Math.min(420, root.width - Tokens.space["6"] * 2)
        status: root.current.status
        title: root.current.title
        message: root.statusMessage
        actionText: root.current.showRetry ? qsTr("Retry") : ""
        onActionTriggered: root.retryRequested()
    }
}

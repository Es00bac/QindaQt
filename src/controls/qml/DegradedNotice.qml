// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick

StateCard {
    id: control

    property string reason: ""
    property string retryText: ""

    signal retryRequested()

    status: StateCard.Warning
    title: qsTr("Feature unavailable")
    message: reason
    actionText: retryText
    Accessible.name: title
    Accessible.description: reason
    Accessible.role: Accessible.AlertMessage
    onActionTriggered: retryRequested()
}

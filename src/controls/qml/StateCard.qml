// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.Control {
    id: control

    enum Status {
        Information,
        Success,
        Warning,
        Error,
        Busy
    }

    property int status: StateCard.Information
    property string title: ""
    property string message: ""
    property string actionText: ""
    property string accessibleDescription: message
    readonly property bool busy: status === StateCard.Busy
    readonly property bool error: status === StateCard.Error
    readonly property bool alert: status === StateCard.Warning || error
    readonly property int politeAnnouncement: Accessible.Polite
    readonly property int assertiveAnnouncement: Accessible.Assertive
    readonly property var semanticPair: status === StateCard.Success ? Tokens.status.success
                                        : status === StateCard.Warning ? Tokens.status.warning
                                        : Tokens.status.info
    readonly property color semanticBackground: error ? Tokens.danger.default
                                                      : semanticPair.background
    readonly property color semanticForeground: error ? Tokens.danger.fg
                                                      : semanticPair.foreground
    readonly property real textContentWidth: Math.max(
        0, availableWidth - (action.visible ? action.implicitWidth + Tokens.space["4"] : 0))

    signal actionTriggered()
    signal accessibilityAnnouncementRequested(string message, int status, int politeness)

    function statusName() {
        if (status === StateCard.Warning)
            return qsTr("Warning")
        if (status === StateCard.Error)
            return qsTr("Error")
        if (status === StateCard.Busy)
            return qsTr("Busy")
        if (status === StateCard.Success)
            return qsTr("Success")
        return qsTr("Information")
    }

    QtObject {
        id: announcementState

        property bool ready: false

        function schedule() {
            if (ready)
                announcementTimer.restart()
        }

        function publishLatest() {
            if (!ready)
                return
            const announcement = qsTr("%1: %2 — %3")
                .arg(control.statusName()).arg(control.title).arg(control.message)
            // Derive urgency from the final tuple, after every binding and
            // caller mutation in this event turn has settled.
            const isAlert = control.status === StateCard.Warning
                || control.status === StateCard.Error
            const politeness = isAlert ? control.assertiveAnnouncement
                                       : control.politeAnnouncement
            // AGENT-CONTRACT: This is the real Qt accessibility announcement
            // path. The paired signal is deterministic offscreen evidence for
            // the identical latest tuple; it is not a substitute AT bridge.
            Accessible.announce(announcement, politeness)
            control.accessibilityAnnouncementRequested(
                announcement, control.status, politeness)
        }
    }

    Timer {
        id: announcementTimer
        interval: 0
        repeat: false
        onTriggered: announcementState.publishLatest()
    }

    Connections {
        target: control
        function onStatusChanged() { announcementState.schedule() }
        function onTitleChanged() { announcementState.schedule() }
        function onMessageChanged() { announcementState.schedule() }
    }

    Component.onCompleted: announcementState.ready = true

    padding: Tokens.space["4"]
    // AGENT-GUARD: The inner text column intentionally has a zero preferred
    // width for wrapping. Preserve a useful outer card preference so parent
    // grids do not treat no-action cards as a one-character column.
    implicitWidth: 220
    Accessible.role: alert ? Accessible.AlertMessage : Accessible.StaticText
    Accessible.name: busy ? qsTr("%1, busy").arg(title) : title
    Accessible.description: busy ? qsTr("Busy. %1").arg(accessibleDescription)
                                 : accessibleDescription

    contentItem: Item {
        implicitHeight: Math.max(textColumn.implicitHeight,
                                 action.visible ? action.implicitHeight : 0)

        Column {
            id: textColumn
            objectName: "stateCardTextColumn"

            // AGENT-GUARD: Bind Text widths directly rather than feeding
            // wrap-dependent implicit widths back through nested Layouts.
            // Retry keeps its native minimum; this column owns the remainder.
            width: control.textContentWidth
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Tokens.space["1"]

            Text {
                objectName: "stateCardTitle"
                width: parent.width
                text: control.busy ? qsTr("%1, busy").arg(control.title) : control.title
                color: control.semanticForeground
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }

            Text {
                objectName: "stateCardMessage"
                width: parent.width
                text: control.message
                color: control.semanticForeground
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }
        }

        Button {
            id: action
            objectName: "stateCardAction"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: control.actionText.length > 0
            available: control.enabled && !control.busy
            emphasized: false
            text: control.actionText
            accessibleDescription: control.accessibleDescription
            onClicked: control.actionTriggered()
        }
    }

    background: Rectangle {
        radius: Tokens.radius.l
        color: control.semanticBackground
        border.width: Tokens.space["1"] / 2
        border.color: control.error ? Tokens.danger.default : Tokens.outline.strong
    }
}

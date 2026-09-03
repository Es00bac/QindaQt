// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Compiled task-list panel strip. The controller is the composed shell facade
// injected above QML as `access`; this file owns no state, no transport, and
// no window authority — every gesture re-enters the controller, which applies
// capability, generation, and pending fences before any dispatch.
//
// AGENT-CONTRACT: presentation consumes semantic QST-1 roles through the
// QindaQt.Tokens singleton and the compiled QindaQt.Controls primitives
// (module-boundaries rule: first-party presentation imports QindaQt.Controls
// explicitly). This file must not grow palette literals, theme-id knowledge,
// or its own fallback colors — Controls/Tokens are the only theme authority.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property string phase: access !== null ? access.phaseText : "unavailable"
    readonly property bool stripVisible: access !== null && access.entryCount > 0

    objectName: "taskListApplet"
    implicitWidth: vertical ? 44 : strip.implicitWidth
    implicitHeight: vertical ? strip.implicitHeight : 32

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Task list")
    Accessible.description: {
        if (access === null)
            return qsTr("Task list controls are not connected")
        if (phase === "loading")
            return qsTr("The task list is loading")
        if (phase === "empty")
            return qsTr("No windows are visible in this scope")
        if (phase === "degraded")
            return qsTr("The task list source is limited; actions are paused")
        if (phase === "unavailable")
            return qsTr("The task list is unavailable")
        return qsTr("%1 windows").arg(access.totalEntryCount)
    }

    GridLayout {
        id: strip
        anchors.fill: parent
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rows: root.vertical ? -1 : 1
        columns: root.vertical ? 1 : -1
        rowSpacing: Tokens.space["1"]
        columnSpacing: Tokens.space["1"]

        C.Label {
            id: loadingLabel
            objectName: "taskListLoadingLabel"
            visible: root.phase === "loading"
            text: qsTr("Loading…")
            muted: true
        }

        C.Label {
            id: unavailableLabel
            objectName: "taskListUnavailableLabel"
            visible: root.phase === "unavailable"
            text: qsTr("Task list unavailable")
            muted: true
            Accessible.name: root.access !== null
                ? qsTr("Task list unavailable: %1").arg(root.access.phaseReasonText)
                : qsTr("Task list unavailable")
        }

        C.Label {
            id: emptyLabel
            objectName: "taskListEmptyLabel"
            visible: root.phase === "empty"
            text: qsTr("No windows")
            muted: true
        }

        Repeater {
            id: entryRepeater
            model: root.stripVisible ? root.access.entryRows : []

            delegate: TaskListEntryButton {
                required property var modelData
                required property int index

                entry: modelData
                access: root.access
                vertical: root.vertical

                KeyNavigation.left: root.vertical
                    ? null : entryRepeater.itemAt(index - 1)
                KeyNavigation.right: root.vertical
                    ? null : entryRepeater.itemAt(index + 1)
                KeyNavigation.up: root.vertical
                    ? entryRepeater.itemAt(index - 1) : null
                KeyNavigation.down: root.vertical
                    ? entryRepeater.itemAt(index + 1) : null
            }
        }

        // Overflow truth: the controller caps presented rows and reports the
        // exact hidden count; the strip never silently drops entries.
        C.Label {
            id: overflowIndicator
            objectName: "taskListOverflowIndicator"
            visible: root.access !== null && root.access.overflowCount > 0
            text: visible ? qsTr("+%1 more").arg(root.access.overflowCount) : ""
            muted: true
            Accessible.name: visible
                ? qsTr("%1 further windows are not shown")
                      .arg(root.access !== null ? root.access.overflowCount : 0)
                : ""
        }

        // Degraded truth stays visible next to the retained rows. The warning
        // status role, never color alone, marks the limitation.
        Text {
            id: degradedBadge
            objectName: "taskListDegradedBadge"
            visible: root.phase === "degraded"
            text: qsTr("Limited")
            color: Tokens.status.warning.foreground
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            Accessible.role: Accessible.StaticText
            Accessible.name: root.access !== null
                ? qsTr("Task list source is limited: %1")
                      .arg(root.access.phaseReasonText)
                : ""
        }
    }

    // Intent feedback (refusals, failures, uncertainty) surfaces as a
    // dismissable non-modal notice with an alert role.
    T.Popup {
        id: feedbackPopup
        objectName: "taskListFeedbackPopup"
        visible: root.access !== null && root.access.feedbackPresent
        modal: false
        focus: visible
        closePolicy: T.Popup.CloseOnEscape
        padding: Tokens.space["2"]
        parent: root

        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)

        background: Rectangle {
            radius: Tokens.radius.m
            color: Tokens.bg.raised
            border.color: Tokens.outline.strong
        }

        contentItem: RowLayout {
            spacing: Tokens.space["2"]

            C.Label {
                id: feedbackText
                objectName: "taskListFeedbackText"
                Layout.maximumWidth: 320
                text: root.access !== null ? root.access.feedback : ""
                Accessible.role: Accessible.AlertMessage
            }

            C.Button {
                id: feedbackDismiss
                objectName: "taskListFeedbackDismiss"
                text: qsTr("Dismiss")
                emphasized: false
                accessibleDescription: qsTr("Dismiss task list notice")
                onClicked: {
                    if (root.access !== null)
                        root.access.clearFeedback()
                }
                Accessible.onPressAction: {
                    if (root.access !== null)
                        root.access.clearFeedback()
                }
            }
        }
    }
}

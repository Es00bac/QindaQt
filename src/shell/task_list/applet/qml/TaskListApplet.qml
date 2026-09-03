// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts

// Compiled task-list panel strip. The controller is the composed shell facade
// injected above QML as `access`; this file owns no state, no transport, and
// no window authority — every gesture re-enters the controller, which applies
// capability, generation, and pending fences before any dispatch.
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false

    readonly property var colors: theme.colors ?? ({})
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
        rowSpacing: 2
        columnSpacing: 2

        T.Label {
            id: loadingLabel
            objectName: "taskListLoadingLabel"
            visible: root.phase === "loading"
            text: qsTr("Loading…")
            color: root.colors.textMuted ?? "#a9afa9"
            font.pixelSize: 11
        }

        T.Label {
            id: unavailableLabel
            objectName: "taskListUnavailableLabel"
            visible: root.phase === "unavailable"
            text: qsTr("Task list unavailable")
            color: root.colors.textMuted ?? "#a9afa9"
            font.pixelSize: 11
            Accessible.role: Accessible.StaticText
            Accessible.name: root.access !== null
                ? qsTr("Task list unavailable: %1").arg(root.access.phaseReasonText)
                : qsTr("Task list unavailable")
        }

        T.Label {
            id: emptyLabel
            objectName: "taskListEmptyLabel"
            visible: root.phase === "empty"
            text: qsTr("No windows")
            color: root.colors.textMuted ?? "#a9afa9"
            font.pixelSize: 11
        }

        Repeater {
            id: entryRepeater
            model: root.stripVisible ? root.access.entryRows : []

            delegate: TaskListEntryButton {
                required property var modelData
                required property int index

                entry: modelData
                access: root.access
                theme: root.theme
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
        T.Label {
            id: overflowIndicator
            objectName: "taskListOverflowIndicator"
            visible: root.access !== null && root.access.overflowCount > 0
            text: visible ? qsTr("+%1 more").arg(root.access.overflowCount) : ""
            color: root.colors.textMuted ?? "#a9afa9"
            font.pixelSize: 11
            Accessible.role: Accessible.StaticText
            Accessible.name: visible
                ? qsTr("%1 further windows are not shown")
                      .arg(root.access !== null ? root.access.overflowCount : 0)
                : ""
        }

        // Degraded truth stays visible next to the retained rows.
        T.Label {
            id: degradedBadge
            objectName: "taskListDegradedBadge"
            visible: root.phase === "degraded"
            text: qsTr("Limited")
            color: root.colors.warning ?? "#e5a84b"
            font.pixelSize: 11
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
        padding: 8
        parent: root

        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 6
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"
        }

        contentItem: RowLayout {
            spacing: 8

            T.Label {
                id: feedbackText
                objectName: "taskListFeedbackText"
                Layout.maximumWidth: 320
                text: root.access !== null ? root.access.feedback : ""
                color: root.colors.text ?? "#f2f1eb"
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            T.Button {
                id: feedbackDismiss
                objectName: "taskListFeedbackDismiss"
                text: qsTr("Dismiss")
                focusPolicy: Qt.TabFocus
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Dismiss task list notice")
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

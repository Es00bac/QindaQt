// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

pragma ComponentBehavior: Bound

// Bounded status-notifier tray applet surface.
// The controller is the composed shell facade injected above QML as `access`;
// this file never imports the StatusNotifier registry, transports, or D-Bus
// and owns no presentation policy beyond layout.
Item {
    id: root

    objectName: "statusNotifierApplet"

    required property var access
    // Theme handle the shell composition passes for future styling hooks;
    // QST-1 token values themselves come from the Tokens singleton.
    required property var theme
    property bool vertical: false

    readonly property bool hasAccess: access !== null && access !== undefined
    readonly property string phase: hasAccess ? String(access.phaseText) : "unavailable"
    readonly property bool showItems: hasAccess && (phase === "ready" || phase === "degraded")

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Status tray")
    Accessible.description: {
        if (!hasAccess)
            return qsTr("Status tray controls are not connected")
        if (phase === "loading")
            return qsTr("Status items are loading")
        if (phase === "empty")
            return qsTr("No status items")
        if (phase === "degraded")
            return qsTr("Status tray is limited: %1").arg(access.phaseReasonText)
        if (phase === "unavailable")
            return qsTr("Status tray is unavailable: %1").arg(access.phaseReasonText)
        return qsTr("%1 status items").arg(access.itemCount)
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Tokens.space["2"]
        // A disconnected surface is visibly disabled, not dead-looking.
        enabled: root.hasAccess

        C.StateCard {
            id: notConnectedState
            objectName: "statusNotifierNotConnectedState"
            Layout.fillWidth: true
            visible: !root.hasAccess
            status: C.StateCard.Warning
            title: qsTr("Status tray")
            message: qsTr("Status tray controls are not connected.")
        }

        C.StateCard {
            id: loadingState
            objectName: "statusNotifierLoadingState"
            Layout.fillWidth: true
            visible: root.hasAccess && root.phase === "loading"
            status: C.StateCard.Busy
            title: qsTr("Status tray")
            message: qsTr("Status items are loading…")
        }

        C.StateCard {
            id: emptyState
            objectName: "statusNotifierEmptyState"
            Layout.fillWidth: true
            visible: root.hasAccess && root.phase === "empty"
            status: C.StateCard.Information
            title: qsTr("Status tray")
            message: qsTr("No status items.")
        }

        C.DegradedNotice {
            id: unavailableNotice
            objectName: "statusNotifierUnavailableNotice"
            Layout.fillWidth: true
            visible: root.hasAccess && root.phase === "unavailable"
            title: qsTr("Status tray is unavailable")
            reason: root.hasAccess ? access.phaseReasonText : ""
        }

        C.DegradedNotice {
            id: degradedNotice
            objectName: "statusNotifierDegradedNotice"
            Layout.fillWidth: true
            visible: root.hasAccess && root.phase === "degraded"
            title: qsTr("Status tray is limited")
            reason: root.hasAccess ? access.phaseReasonText : ""
        }

        Loader {
            id: stripLoader
            objectName: "statusNotifierStripLoader"
            visible: root.showItems
            sourceComponent: root.vertical ? verticalStrip : horizontalStrip
        }

        C.StateCard {
            id: feedbackCard
            objectName: "statusNotifierFeedbackCard"
            Layout.fillWidth: true
            visible: root.hasAccess && (access.feedbackPresent === true)
            status: C.StateCard.Error
            title: qsTr("Status Tray Notice")
            message: root.hasAccess ? access.feedback : ""
            actionText: qsTr("Dismiss")
            onActionTriggered: {
                if (root.hasAccess) {
                    access.clearFeedback()
                }
            }
        }
    }

    Component {
        id: overflowChipComponent

        // Truthful overflow indicator, not an action: the excess items are
        // not projected, so the chip announces the count and offers nothing.
        T.Control {
            objectName: "statusNotifierOverflowChip"
            visible: root.hasAccess && access.overflowCount > 0
            padding: Tokens.space["2"]

            Accessible.role: Accessible.StaticText
            Accessible.name: root.hasAccess ? access.overflowText : ""

            contentItem: Text {
                text: root.hasAccess ? access.overflowText : ""
                color: Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
            }

            background: Rectangle {
                radius: Tokens.radius.m
                color: Tokens.bg.raised
                border.width: Tokens.space["1"] / 2
                border.color: Tokens.outline.divider
            }
        }
    }

    Component {
        id: horizontalStrip

        Row {
            spacing: Tokens.space["1"]

            Repeater {
                objectName: "statusNotifierStripRepeater"
                model: root.hasAccess ? access.itemRows : []

                delegate: StatusNotifierItemDelegate {
                    // AGENT-GUARD: compiled QML resolves model context only for
                    // explicitly declared delegate properties; an implicit
                    // `modelData` reference is a ReferenceError in the compiled
                    // module and leaves every row's `item` undefined.
                    required property var modelData
                    item: modelData
                    access: root.access
                    iconSize: root.hasAccess ? access.iconSize : 22
                }
            }

            Loader {
                sourceComponent: overflowChipComponent
            }
        }
    }

    Component {
        id: verticalStrip

        Column {
            spacing: Tokens.space["1"]

            Repeater {
                objectName: "statusNotifierStripRepeater"
                model: root.hasAccess ? access.itemRows : []

                delegate: StatusNotifierItemDelegate {
                    // AGENT-GUARD: see the horizontal strip — compiled QML
                    // requires the explicit modelData declaration.
                    required property var modelData
                    item: modelData
                    access: root.access
                    iconSize: root.hasAccess ? access.iconSize : 22
                }
            }

            Loader {
                sourceComponent: overflowChipComponent
            }
        }
    }
}

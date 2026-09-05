// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
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
        if (access.feedbackPresent)
            return qsTr("Status tray notice: %1").arg(access.feedback)
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
        // Panel chrome contains status-item icons only. Phase truth remains
        // available through the root accessible description; diagnostics do
        // not expand the panel into a text card.
        enabled: root.hasAccess

        C.StateCard {
            id: notConnectedState
            objectName: "statusNotifierNotConnectedState"
            Layout.fillWidth: true
            visible: false
            status: C.StateCard.Warning
            title: qsTr("Status tray")
            message: qsTr("Status tray controls are not connected.")
        }

        C.StateCard {
            id: loadingState
            objectName: "statusNotifierLoadingState"
            Layout.fillWidth: true
            visible: false
            status: C.StateCard.Busy
            title: qsTr("Status tray")
            message: qsTr("Status items are loading…")
        }

        C.StateCard {
            id: emptyState
            objectName: "statusNotifierEmptyState"
            Layout.fillWidth: true
            visible: false
            status: C.StateCard.Information
            title: qsTr("Status tray")
            message: qsTr("No status items.")
        }

        C.DegradedNotice {
            id: unavailableNotice
            objectName: "statusNotifierUnavailableNotice"
            Layout.fillWidth: true
            visible: false
            title: qsTr("Status tray is unavailable")
            reason: root.hasAccess ? access.phaseReasonText : ""
        }

        C.DegradedNotice {
            id: degradedNotice
            objectName: "statusNotifierDegradedNotice"
            Layout.fillWidth: true
            visible: false
            title: qsTr("Status tray is limited")
            reason: root.hasAccess ? access.phaseReasonText : ""
        }

        Loader {
            id: stripLoader
            objectName: "statusNotifierStripLoader"
            visible: root.showItems
            sourceComponent: root.vertical ? verticalStrip : horizontalStrip
        }

    }

    // Operation feedback is a popup, so it remains actionable without ever
    // expanding the panel strip into a text card.
    T.Popup {
        id: feedbackPopup
        objectName: "statusNotifierFeedbackPopup"
        visible: root.hasAccess && access.feedbackPresent
        popupType: T.Popup.Window
        modal: false
        focus: visible
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
                     | T.Popup.CloseOnPressOutsideParent
        padding: Tokens.space["2"]

        // AGENT-GUARD: RuntimePanel rejects focus. This popup window must seed
        // focus itself or its Dismiss action is unreachable from the panel.
        onOpened: Qt.callLater(function() {
            feedbackDismissButton.forceActiveFocus(Qt.PopupFocusReason)
        })

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            C.StateCard {
                id: feedbackCard
                objectName: "statusNotifierFeedbackCard"
                Layout.fillWidth: true
                visible: true
                status: C.StateCard.Error
                title: qsTr("Status Tray Notice")
                message: root.hasAccess ? access.feedback : ""
            }

            T.Button {
                id: feedbackDismissButton
                objectName: "statusNotifierFeedbackDismiss"
                Layout.alignment: Qt.AlignRight
                text: qsTr("Dismiss")
                Accessible.role: Accessible.Button
                Accessible.name: text
                onClicked: {
                if (root.hasAccess)
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

            implicitWidth: 32
            implicitHeight: 28

            contentItem: ShellIcons.Icon {
                name: "view-more-symbolic"
                size: 18
                color: Tokens.fg.muted
                symbolic: true
                fallbackText: qsTr("More")
                Accessible.ignored: true
            }

            background: Item {}
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

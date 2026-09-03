// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var powerSettings
    signal closeRequested()

    // AGENT-GUARD: Host entry must never nominate a disabled action. Domain
    // controls come from admission truth; Retry and Close are safe fallbacks.
    readonly property Item firstFocusTarget:
        root.powerSettings.loading || root.powerSettings.unavailable
            || root.powerSettings.stale
        ? (retryButton.visible && retryButton.enabled ? retryButton : closeButton)
        : profileSection.firstActionTarget !== null
        ? profileSection.firstActionTarget
        : brightnessSection.firstActionTarget !== null
          ? brightnessSection.firstActionTarget
          : sessionSection.firstActionTarget !== null
            ? sessionSection.firstActionTarget
          : retryButton.visible && retryButton.enabled ? retryButton
          : closeButton

    title: qsTr("Power")
    background: Rectangle { color: Tokens.bg.base }

    Keys.onPressed: event => {
        const pageStep = Math.max(1, viewport.height - Tokens.space["5"])
        if (event.key === Qt.Key_PageDown) {
            viewport.contentY = Math.min(
                        Math.max(0, viewport.contentHeight - viewport.height),
                        viewport.contentY + pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            viewport.contentY = Math.max(0, viewport.contentY - pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_Home
                   && (event.modifiers & Qt.ControlModifier)) {
            viewport.contentY = 0
            event.accepted = true
        } else if (event.key === Qt.Key_End
                   && (event.modifiers & Qt.ControlModifier)) {
            viewport.contentY = Math.max(0, viewport.contentHeight - viewport.height)
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "powerPageHeading"
            Layout.fillWidth: true
            text: qsTr("Power and brightness")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        StateCard {
            objectName: "powerServiceState"
            Layout.fillWidth: true
            status: root.powerSettings.loading ? StateCard.Busy
                    : root.powerSettings.ready ? StateCard.Success
                    : root.powerSettings.degraded || root.powerSettings.stale
                      ? StateCard.Warning : StateCard.Error
            title: root.powerSettings.stale ? qsTr("Stale power information")
                   : root.powerSettings.degraded ? qsTr("Limited power information")
                   : root.powerSettings.ready ? qsTr("Power service ready")
                   : qsTr("Power service unavailable")
            message: root.powerSettings.statusText
        }

        Label {
            objectName: "powerOperationStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.powerSettings.operationStatusText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "powerError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.powerSettings.errorText
            color: Tokens.status.warning.foreground
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "powerFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Power settings scroll position")
            }

            function revealItem(item) {
                if (item === null || item === undefined) return
                let cursor = item
                let belongsToForm = false
                while (cursor !== null && cursor !== undefined) {
                    if (cursor === formSurface) { belongsToForm = true; break }
                    cursor = cursor.parent
                }
                if (!belongsToForm) return
                const position = item.mapToItem(formSurface, 0, 0)
                const margin = Tokens.space["2"]
                if (position.y - margin < contentY)
                    contentY = Math.max(0, position.y - margin)
                else if (position.y + item.height + margin > contentY + height)
                    contentY = Math.min(Math.max(0, contentHeight - height),
                                        position.y + item.height + margin - height)
            }

            FormSurface {
                id: formSurface
                width: parent.width
                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["4"]
                    PowerSupplySection { powerSettings: root.powerSettings }
                    PowerProfileSection {
                        id: profileSection
                        powerSettings: root.powerSettings
                    }
                    PowerBrightnessSection {
                        id: brightnessSection
                        powerSettings: root.powerSettings
                    }
                    PowerSessionSection {
                        id: sessionSection
                        sessionActions: root.powerSettings.sessionActions
                    }
                }
            }
        }

        Connections {
            target: root.Window.window
            enabled: root.Window.window !== null
            function onActiveFocusItemChanged() {
                if (root.Window.window !== null)
                    viewport.revealItem(root.Window.window.activeFocusItem)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]
            Button {
                id: retryButton
                objectName: "powerRetryButton"
                visible: !root.powerSettings.ready
                available: root.powerSettings.retryAvailable
                busy: root.powerSettings.loading
                emphasized: false
                text: qsTr("Retry")
                accessibleDescription: qsTr("Reconnect to Power1 and reload authoritative state")
                onClicked: root.powerSettings.retry()
            }
            Label {
                Layout.fillWidth: true
                text: root.powerSettings.busy ? qsTr("A power change is pending.")
                      : root.powerSettings.serviceEpoch > 0
                        ? qsTr("Epoch %1, revision %2")
                          .arg(root.powerSettings.serviceEpoch)
                          .arg(root.powerSettings.serviceRevision) : ""
                muted: true
                Accessible.name: text
            }
            Button {
                id: closeButton
                objectName: "powerCloseButton"
                available: true
                busy: false
                emphasized: false
                text: qsTr("Close")
                KeyNavigation.tab: root.firstFocusTarget
                onClicked: root.closeRequested()
            }
        }
    }
}

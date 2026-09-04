// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root

    required property var colorSettings
    signal closeRequested()

    // AGENT-GUARD: Host entry must never nominate a disabled action. Domain
    // controls come from admission truth; Import, Retry, and Close are the
    // safe fallbacks and Close is always admitted.
    readonly property Item firstFocusTarget:
        root.colorSettings.loading || root.colorSettings.unavailable
            || root.colorSettings.stale
        ? (retryButton.visible && retryButton.enabled ? retryButton : closeButton)
        : profileSection.firstActionTarget !== null
        ? profileSection.firstActionTarget
        : outputSection.firstActionTarget !== null
          ? outputSection.firstActionTarget
          : importSection.importTarget
        ? importSection.importTarget
        : retryButton.visible && retryButton.enabled ? retryButton
        : closeButton

    title: qsTr("Color")
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
            objectName: "colorPageHeading"
            Layout.fillWidth: true
            text: qsTr("Display color profiles")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        StateCard {
            objectName: "colorServiceState"
            Layout.fillWidth: true
            status: root.colorSettings.loading ? StateCard.Busy
                    : root.colorSettings.ready ? StateCard.Success
                    : root.colorSettings.degraded || root.colorSettings.stale
                      ? StateCard.Warning : StateCard.Error
            title: root.colorSettings.stale ? qsTr("Stale color assignments")
                   : root.colorSettings.degraded ? qsTr("Limited color information")
                   : root.colorSettings.ready ? qsTr("Color settings ready")
                   : qsTr("Color services unavailable")
            message: root.colorSettings.statusText
        }

        Label {
            objectName: "colorOperationStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.colorSettings.operationStatusText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "colorError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.colorSettings.errorText
            color: Tokens.status.warning.foreground
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        StateCard {
            objectName: "colorAuthorityBoundary"
            Layout.fillWidth: true
            status: StateCard.Information
            title: qsTr("Assignments are stored intents")
            message: qsTr("This page records which ICC profile each display should use. It does not apply profiles to the compositor or to displays; color application is a separate authority.")
        }

        Flickable {
            id: viewport
            objectName: "colorFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Color settings scroll position")
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
                    ColorOutputSection {
                        id: outputSection
                        colorSettings: root.colorSettings
                    }
                    ColorProfileSection {
                        id: profileSection
                        colorSettings: root.colorSettings
                    }
                    ColorImportSection {
                        id: importSection
                        colorSettings: root.colorSettings
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
                objectName: "colorRetryButton"
                visible: !root.colorSettings.ready
                available: root.colorSettings.retryAvailable
                busy: root.colorSettings.loading
                emphasized: false
                text: qsTr("Retry")
                accessibleDescription: qsTr("Reconnect to the display and settings services and rescan the profile catalog")
                onClicked: root.colorSettings.retry()
            }
            Label {
                Layout.fillWidth: true
                text: root.colorSettings.busy ? qsTr("A color change is being saved.")
                      : root.colorSettings.displayRevision > 0
                        ? qsTr("Display revision %1, settings revision %2")
                          .arg(root.colorSettings.displayRevision)
                          .arg(root.colorSettings.settingsRevision) : ""
                muted: true
                Accessible.name: text
            }
            Button {
                id: closeButton
                objectName: "colorCloseButton"
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

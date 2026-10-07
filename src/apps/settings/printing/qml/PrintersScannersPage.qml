// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root
    required property var printingSettings
    signal closeRequested()
    readonly property Item firstFocusTarget:
        printerCard.actionButton.enabled ? printerCard.actionButton
        : cupsCard.actionButton.enabled ? cupsCard.actionButton
        : scannerCard.actionButton.enabled ? scannerCard.actionButton : refreshButton
    title: qsTr("Printers & scanners")
    background: Rectangle { color: Tokens.bg.base }

    function revealCard(card) {
        const top = card.mapToItem(formSurface, 0, 0).y
        const bottom = top + card.height
        if (top < viewport.contentY) viewport.contentY = Math.max(0, top)
        else if (bottom > viewport.contentY + viewport.height)
            viewport.contentY = Math.min(Math.max(0, viewport.contentHeight - viewport.height),
                                         bottom - viewport.height)
    }
    Keys.onPressed: event => {
        const step = Math.max(1, viewport.height - Tokens.space["4"])
        if (event.key === Qt.Key_PageDown || event.key === Qt.Key_PageUp) {
            viewport.contentY = Math.max(0, Math.min(
                Math.max(0, viewport.contentHeight - viewport.height),
                viewport.contentY + (event.key === Qt.Key_PageDown ? step : -step)))
            event.accepted = true
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["4"]
        spacing: Tokens.space["3"]
        Label {
            objectName: "printingPageHeading"
            Layout.fillWidth: true
            text: root.title
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Open the installed tools to set up printers or scan documents. They show connection and device status.")
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            muted: true
        }
        Label {
            objectName: "printingLaunchStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.printingSettings.statusText
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.StatusBar
            Accessible.name: text
        }
        Flickable {
            id: viewport
            objectName: "printingFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false
            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
            }
            FormSurface {
                id: formSurface
                width: parent.width
                padding: Tokens.space["3"]
                contentItem: ColumnLayout {
                    spacing: Tokens.space["4"]
                    PrintingToolCard {
                        id: printerCard
                        Layout.fillWidth: true
                        toolRow: root.printingSettings.tools[0]
                        busy: root.printingSettings.busy
                        nextFocusTarget: cupsCard.actionButton.enabled ? cupsCard.actionButton
                            : scannerCard.actionButton.enabled ? scannerCard.actionButton : refreshButton
                        previousFocusTarget: refreshButton
                        onLaunchRequested: toolId => root.printingSettings.launchTool(toolId)
                        onFocusEntered: root.revealCard(printerCard)
                    }
                    PrintingToolCard {
                        id: cupsCard
                        Layout.fillWidth: true
                        toolRow: root.printingSettings.tools[1]
                        busy: root.printingSettings.busy
                        nextFocusTarget: scannerCard.actionButton.enabled
                            ? scannerCard.actionButton : refreshButton
                        previousFocusTarget: printerCard.actionButton.enabled
                            ? printerCard.actionButton : refreshButton
                        onLaunchRequested: toolId => root.printingSettings.launchTool(toolId)
                        onFocusEntered: root.revealCard(cupsCard)
                    }
                    PrintingToolCard {
                        id: scannerCard
                        Layout.fillWidth: true
                        toolRow: root.printingSettings.tools[2]
                        busy: root.printingSettings.busy
                        nextFocusTarget: refreshButton
                        previousFocusTarget: cupsCard.actionButton.enabled ? cupsCard.actionButton
                            : printerCard.actionButton.enabled ? printerCard.actionButton : refreshButton
                        onLaunchRequested: toolId => root.printingSettings.launchTool(toolId)
                        onFocusEntered: root.revealCard(scannerCard)
                    }
                }
            }
        }
        Button {
            id: refreshButton
            objectName: "printingRefreshButton"
            Layout.fillWidth: true
            text: qsTr("Refresh installed tools")
            busy: root.printingSettings.busy
            accessibleDescription: qsTr("Try again after installing or repairing a printer or scanner application")
            KeyNavigation.tab: root.firstFocusTarget
            KeyNavigation.backtab: scannerCard.actionButton.enabled ? scannerCard.actionButton
                : cupsCard.actionButton.enabled ? cupsCard.actionButton
                : printerCard.actionButton.enabled ? printerCard.actionButton : refreshButton
            Keys.onReturnPressed: event => {
                if (enabled) root.printingSettings.refresh()
                event.accepted = true
            }
            Keys.onEnterPressed: event => {
                if (enabled) root.printingSettings.refresh()
                event.accepted = true
            }
            onClicked: root.printingSettings.refresh()
        }
    }
}

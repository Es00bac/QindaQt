// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0
Item {
    id: root
    required property var access
    required property var theme
    property bool vertical: false
    objectName: "agentUsageApplet"
    implicitWidth: 32
    implicitHeight: 28
    ToolButton {
        id: summary
        objectName: "agentUsageAppletSummary"
        anchors.fill: parent
        focusPolicy: Qt.TabFocus
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("AI agent usage")
        Accessible.description: qsTr("Open remaining limits, reset times, token totals and reported cost")
        onClicked: details.open()
        Accessible.onPressAction: details.open()
        contentItem: ShellIcons.Icon {
            objectName: "agentUsageIcon"
            name: "applications-development"
            size: Math.min(20, root.height - 8)
            color: Tokens.fg.default
            symbolic: true
            fallbackText: qsTr("AI")
            Accessible.ignored: true
        }
        background: Item {}
    }
    // AGENT-GUARD: freshness is a source snapshot projection only. Polling
    // must never refresh provider processes or reports (ADR-0351).
    Timer {
        objectName: "agentUsageFreshnessTimer"
        interval: 60000
        repeat: true
        running: details.opened && root.access !== null && root.access.readGranted
        onTriggered: root.access.checkFreshness()
    }
    C.PanelPopup {
        id: details
        objectName: "agentUsageAppletPopup"
        anchorItem: summary
        vertical: root.vertical
        width: Math.min(430, Math.max(240, summary.Screen.width - 24))
        padding: 12
        onOpened: { if (root.access !== null) root.access.refresh() }
        onClosed: summary.forceActiveFocus(Qt.PopupFocusReason)
        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }
        contentItem: ScrollView {
            implicitHeight: Math.min(content.implicitHeight, 520,
                                     Math.max(120, summary.Screen.height - 48))
            clip: true
            Shortcut {
                sequence: "Escape"
                context: Qt.WindowShortcut
                enabled: details.opened
                onActivated: details.close()
            }
            ColumnLayout {
                id: content
                width: details.availableWidth
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: qsTr("AI agent usage")
                        color: Tokens.fg.default
                        font.bold: true
                        Accessible.role: Accessible.Heading
                    }
                    C.Button {
                        objectName: "agentUsageRefresh"
                        text: qsTr("Refresh")
                        emphasized: false
                        available: root.access !== null && root.access.readGranted
                        accessibleDescription: qsTr("Refresh agent usage")
                        onClicked: root.access.refresh()
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    objectName: "agentUsageDiagnostic"
                    Layout.fillWidth: true
                    text: root.access !== null ? root.access.diagnostic : qsTr("Agent usage is unavailable.")
                    color: Tokens.fg.muted
                    wrapMode: Text.Wrap
                }
                Repeater {
                    model: root.access !== null ? root.access.providerRows : []
                    AgentUsageProviderRow {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    objectName: "agentUsageSetup"
                    Layout.fillWidth: true
                    text: qsTr("Provider setup: qindaqt-agent-usage-report.\nGuide: QindaQt wiki → AI agent usage applet.")
                    color: Tokens.fg.muted
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}

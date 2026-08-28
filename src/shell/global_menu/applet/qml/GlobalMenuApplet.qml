// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// AGENT-NOTE: consumes only GlobalMenuAppletAccess's public Q_PROPERTY/
// Q_INVOKABLE surface (see applet/include/.../globalmenuappletaccess.h).
// G0 wires no live publisher anywhere in the shell, so `available` stays
// false and this renders the same unavailable placeholder as an unprovisioned
// notification center; do not read this component's presence as a live
// global menu.
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null && Boolean(access.available)
    readonly property var topLevelItems: available ? (access.items ?? []) : []

    objectName: "globalMenuApplet"
    implicitWidth: vertical ? 40 : (available ? row.implicitWidth : placeholder.implicitWidth + 16)
    implicitHeight: 28
    Accessible.role: Accessible.MenuBar
    Accessible.name: available ? qsTr("Application menu") : qsTr("Menu unavailable")

    Text {
        id: placeholder
        anchors.centerIn: parent
        visible: !root.available
        text: qsTr("Menu unavailable")
        textFormat: Text.PlainText
        color: root.colors.textMuted ?? "#a9afa9"
        font.pixelSize: 12
    }

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        visible: root.available
        spacing: 12

        Repeater {
            model: root.topLevelItems

            delegate: MouseArea {
                id: entry
                required property var modelData
                readonly property bool itemEnabled: Boolean(modelData.enabled)

                objectName: "globalMenuTopLevelItem"
                width: label.implicitWidth + 12
                height: 24
                enabled: itemEnabled
                cursorShape: itemEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                Accessible.role: Accessible.MenuItem
                Accessible.name: String(modelData.text ?? "")

                onClicked: root.access.activate(modelData.id)

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: String(entry.modelData.text ?? "")
                    textFormat: Text.PlainText
                    color: entry.itemEnabled ? (root.colors.text ?? "white")
                                              : (root.colors.textMuted ?? "#a9afa9")
                    font.pixelSize: 12
                }
            }
        }
    }
}

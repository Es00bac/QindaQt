// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// AGENT-NOTE: consumes only GlobalMenuAppletAccess's public Q_PROPERTY/
// Q_INVOKABLE surface (see applet/include/.../globalmenuappletaccess.h).
// G0 wires no live publisher anywhere in the shell, so `available` stays
// false and this renders the same unavailable placeholder as an unprovisioned
// notification center; do not read this component's presence as a live
// global menu.
//
// AGENT-GUARD: top-level "submenu" entries must stay visibly present but
// non-activating (disabled, no pointer cursor, no activation call) — G0 has
// no submenu popup, and rendering them as clickable fakes would pretend an
// interaction that does not exist. Only enabled "action" entries activate.
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    // Bounded narrow-panel behavior: entries beyond the limit collapse into a
    // muted "+N" indicator, and the root clips so a constrained panel can
    // never paint past its assigned geometry.
    property int maximumVisibleEntries: 8
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null && Boolean(access.available)
    readonly property var topLevelItems: available ? (access.items ?? []) : []
    readonly property var visibleEntries: topLevelItems.slice(0, maximumVisibleEntries)
    readonly property int overflowCount: topLevelItems.length - visibleEntries.length

    objectName: "globalMenuApplet"
    implicitWidth: vertical ? 40
                 : available ? row.implicitWidth + (overflowCount > 0 ? overflowIndicator.implicitWidth + spacing : 0)
                 : placeholder.implicitWidth + 16
    implicitHeight: vertical ? (available ? verticalLayout.implicitHeight : 28) : 28
    clip: true
    Accessible.role: Accessible.MenuBar
    Accessible.name: available ? qsTr("Application menu") : qsTr("Menu unavailable")

    readonly property real spacing: 12

    component MenuEntry: AbstractButton {
        id: entry

        required property var modelData

        readonly property bool isAction: String(modelData.kind ?? "action") === "action"
        readonly property bool itemEnabled: isAction && Boolean(modelData.enabled)

        objectName: "globalMenuTopLevelItem"
        focusPolicy: Qt.TabFocus
        enabled: itemEnabled
        Accessible.role: Accessible.MenuItem
        Accessible.focusable: true
        Accessible.name: String(modelData.text ?? "") + (isAction ? "" : qsTr(" (submenu unavailable)"))

        contentItem: Text {
            text: String(entry.modelData.text ?? "")
            textFormat: Text.PlainText
            elide: Text.ElideRight
            maximumLineCount: 1
            color: entry.itemEnabled ? (root.colors.text ?? "white")
                                     : (root.colors.textMuted ?? "#a9afa9")
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Item {}

        // AGENT-GUARD: AbstractButton suppresses clicked() and keyboard
        // activation while disabled, but an assistive-technology press action
        // has no such gate; the explicit enabled check keeps non-activating
        // entries (disabled actions, G0 submenus) honest.
        onClicked: root.access.activate(entry.modelData.id)
        Accessible.onPressAction: {
            if (entry.enabled) {
                root.access.activate(entry.modelData.id);
            }
        }
    }

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
        visible: root.available && !root.vertical
        spacing: root.spacing

        Repeater {
            model: root.visibleEntries

            delegate: MenuEntry {}
        }
    }

    Column {
        id: verticalLayout
        objectName: "globalMenuVerticalLayout"
        anchors.horizontalCenter: parent.horizontalCenter
        visible: root.available && root.vertical
        spacing: 4

        Repeater {
            model: root.visibleEntries

            delegate: MenuEntry {}
        }
    }

    Text {
        id: overflowIndicator
        objectName: "globalMenuOverflowIndicator"
        visible: root.available && root.overflowCount > 0
        text: qsTr("+%1").arg(root.overflowCount)
        textFormat: Text.PlainText
        color: root.colors.textMuted ?? "#a9afa9"
        font.pixelSize: 12
        anchors.left: root.vertical ? undefined : row.right
        anchors.leftMargin: root.spacing
        anchors.verticalCenter: root.vertical ? undefined : row.verticalCenter
        anchors.top: root.vertical ? verticalLayout.bottom : undefined
        anchors.topMargin: 4
        anchors.horizontalCenter: root.vertical ? verticalLayout.horizontalCenter : undefined
    }
}

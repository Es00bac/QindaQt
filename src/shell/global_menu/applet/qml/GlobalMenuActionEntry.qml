// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// AGENT-CONTRACT: one top-level "action" entry for GlobalMenuApplet.qml.
// Width-pressure geometry and text measurement stay owned by the applet and
// arrive here as function properties so this delegate renders exactly the
// extents the applet's layout model computed; do not re-measure locally.
AbstractButton {
    id: root

    required property var modelData
    required property int index
    required property bool vertical
    required property bool available
    required property var access
    required property var colors
    required property font entryFont
    required property real hostWidth
    required property real hostHeight
    required property var entryOffsetFor
    required property var measuredEntryWidthFor
    required property var horizontalEntryHeightFor

    readonly property bool isAction:
        String(modelData.kind ?? "action") === "action"
    readonly property bool itemEnabled: Boolean(modelData.enabled)

    function pressAction() {
        if (enabled)
            access.activate(String(modelData.id ?? ""),
                            String(modelData.generation ?? ""))
    }

    objectName: "globalMenuTopLevelItem"
    visible: isAction
    x: vertical ? Math.round((hostWidth - width) / 2)
                : entryOffsetFor(index)
    y: vertical ? entryOffsetFor(index)
                : Math.round((hostHeight - height) / 2)
    enabled: visible && itemEnabled && available
    implicitWidth: measuredEntryWidthFor(modelData)
    implicitHeight: vertical ? 24 : horizontalEntryHeightFor()
    focusPolicy: Qt.TabFocus
    checkable: false
    checked: Boolean(modelData.checked ?? false)
    Accessible.role: Accessible.MenuItem
    Accessible.focusable: enabled
    Accessible.name: String(modelData.text ?? "")
    Accessible.checkable: Boolean(modelData.checkable ?? false)
    Accessible.checked: Boolean(modelData.checked ?? false)
    // Press activation is intentional: an already-open native popup may
    // consume the corresponding release at its grab boundary. The pressed
    // edge occurs exactly once for pointer and keyboard gestures, matching
    // MenuBarItem itself.
    onPressedChanged: {
        if (pressed)
            pressAction()
    }
    Accessible.onPressAction: pressAction()
    Keys.onReturnPressed: pressAction()
    Keys.onEnterPressed: pressAction()

    contentItem: Text {
        text: String(root.modelData.text ?? "")
        textFormat: Text.PlainText
        elide: Text.ElideNone
        maximumLineCount: 1
        color: root.itemEnabled
            ? (root.colors.text ?? "white")
            : (root.colors.textMuted ?? "#a9afa9")
        font: root.entryFont
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Item {}
}

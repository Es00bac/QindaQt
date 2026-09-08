// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

pragma ComponentBehavior: Bound

// One status-notifier tray item button. Dispatches user intents (activate,
// context menu) to the controller facade as generation-fenced exact-owner
// keys; it never executes anything itself.
T.Control {
    id: delegateRoot

    objectName: "statusNotifierItemDelegate"

    required property var item
    required property var access
    property int iconSize: 22

    property var menuState: ({status: "none", revision: "0", entries: []})

    readonly property bool hasAccess: access !== null && access !== undefined

    Accessible.role: Accessible.Button
    Accessible.name: item ? item.accessibleName : ""
    Accessible.description: {
        if (!item)
            return ""
        var parts = []
        if (item.accessibleStatusText.length > 0)
            parts.push(item.accessibleStatusText)
        if (item.accessibleDescription.length > 0)
            parts.push(item.accessibleDescription)
        if (item.keyboardActivateText.length > 0)
            parts.push(qsTr("Activate: %1").arg(item.keyboardActivateText))
        if (item.keyboardContextMenuText.length > 0)
            parts.push(qsTr("Context menu: %1").arg(item.keyboardContextMenuText))
        return parts.join("; ")
    }

    implicitWidth: delegateRoot.iconSize + leftPadding + rightPadding
    implicitHeight: delegateRoot.iconSize + topPadding + bottomPadding
    padding: Tokens.space["2"]

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    function anchorPosition() {
        return mapToGlobal(width / 2, height / 2)
    }

    function activate() {
        if (!hasAccess || !item)
            return
        if (access.itemIsMenu(item.uniqueName, item.objectPath, item.generation)) {
            openContextPopup()
        } else {
            const point = anchorPosition()
            access.activateItem(item.uniqueName, item.objectPath, item.generation,
                                Math.round(point.x), Math.round(point.y))
        }
    }

    function updateMenu() {
        if (hasAccess && item)
            menuState = access.menuStateFor(item.uniqueName, item.objectPath, item.generation)
    }

    function openContextPopup() {
        if (!hasAccess || !item)
            return
        const exported = access.hasExportedMenu(item.uniqueName, item.objectPath, item.generation)
        const point = anchorPosition()
        if (!access.openContextMenu(item.uniqueName, item.objectPath, item.generation,
                                    Math.round(point.x), Math.round(point.y)))
            return
        // Items without an exported menu render their own native context menu.
        if (exported) {
            updateMenu()
            contextPopup.popup(0, height)
        }
    }

    Connections {
        target: delegateRoot.hasAccess ? delegateRoot.access : null
        function onMenuChanged() { delegateRoot.updateMenu() }
    }

    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onSpacePressed: activate()
    Keys.onPressed: (event) => {
        if ((event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier) !== 0)
                || event.key === Qt.Key_Menu) {
            openContextPopup()
            event.accepted = true
        }
    }

    contentItem: Item {
        implicitWidth: delegateRoot.iconSize
        implicitHeight: delegateRoot.iconSize

        Image {
            id: iconImage
            objectName: "statusNotifierItemIcon"
            anchors.centerIn: parent
            width: delegateRoot.iconSize
            height: delegateRoot.iconSize
            source: delegateRoot.item ? delegateRoot.item.iconDataUrl : ""
            visible: delegateRoot.item !== null
                     && delegateRoot.item !== undefined
                     && delegateRoot.item.iconDataUrl.length > 0
                     && !delegateRoot.item.iconIsPlaceholder
            Accessible.ignored: true
        }

        // Truthful placeholder presentation: a muted badge, never a forged
        // icon. The delegate's accessible strings already carry the status.
        Rectangle {
            id: placeholderBadge
            objectName: "statusNotifierIconPlaceholder"
            anchors.centerIn: parent
            width: delegateRoot.iconSize
            height: delegateRoot.iconSize
            radius: Tokens.radius.s
            color: Tokens.bg.highest
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.strong
            visible: !iconImage.visible
            Accessible.ignored: true

            Text {
                anchors.centerIn: parent
                text: delegateRoot.item && delegateRoot.item.identity.length > 0
                      ? delegateRoot.item.identity.charAt(0).toUpperCase()
                      : "?"
                color: Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                font.weight: Font.Bold
            }
        }

        Rectangle {
            id: attentionBadge
            objectName: "statusNotifierAttentionBadge"
            anchors.top: parent.top
            anchors.right: parent.right
            width: Tokens.space["2"]
            height: width
            radius: width / 2
            color: Tokens.danger.default
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.bg.raised
            visible: delegateRoot.item !== null && delegateRoot.item !== undefined
                     && delegateRoot.item.needsAttention
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: clickArea.pressed ? Tokens.state.pressed
             : delegateRoot.hovered ? Tokens.state.hover
             : "transparent"
        border.width: delegateRoot.activeFocus ? Tokens.space["1"] : 0
        border.color: Tokens.focus.ring
    }

    T.ToolTip.visible: delegateRoot.hovered
    T.ToolTip.delay: 500
    T.ToolTip.text: delegateRoot.item
        ? (delegateRoot.item.title.length > 0
           ? delegateRoot.item.title
           : delegateRoot.item.identity)
        : ""

    // AGENT-GUARD: this icon-only control has no interactive content children.
    // Put its input area above the Control background: a negative z value lets
    // the background swallow primary/middle clicks before the area sees them.
    MouseArea {
        id: clickArea
        objectName: "statusNotifierItemClickArea"
        anchors.fill: parent
        z: 1
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.MiddleButton && delegateRoot.hasAccess && delegateRoot.item) {
                const point = delegateRoot.anchorPosition()
                delegateRoot.access.secondaryActivateItem(delegateRoot.item.uniqueName,
                    delegateRoot.item.objectPath, delegateRoot.item.generation,
                    Math.round(point.x), Math.round(point.y))
            } else {
                delegateRoot.activate()
            }
        }
        onWheel: (wheel) => {
            if (!delegateRoot.hasAccess || !delegateRoot.item)
                return
            const horizontal = Math.abs(wheel.angleDelta.x) > Math.abs(wheel.angleDelta.y)
            const delta = horizontal ? wheel.angleDelta.x : wheel.angleDelta.y
            if (delta !== 0) {
                wheel.accepted = delegateRoot.access.scrollItem(delegateRoot.item.uniqueName,
                    delegateRoot.item.objectPath, delegateRoot.item.generation,
                    delta, horizontal ? "horizontal" : "vertical")
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: delegateRoot.openContextPopup()
    }

    StatusNotifierMenu {
        id: contextPopup
        objectName: "statusNotifierContextPopup"
        access: delegateRoot.access
        targetItem: delegateRoot.item
        menuData: ({ entries: delegateRoot.menuState.entries ?? [] })
        revision: String(delegateRoot.menuState.revision ?? "0")
        status: String(delegateRoot.menuState.status ?? "none")
    }
}

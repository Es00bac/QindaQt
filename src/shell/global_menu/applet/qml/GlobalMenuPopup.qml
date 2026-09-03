// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Popup {
    id: popup

    required property var access
    required property var theme
    property int maximumDepth: 6
    property var menuStack: []
    property Item anchorItem: null
    readonly property var colors: theme.colors ?? ({})
    readonly property var currentMenu:
        menuStack.length > 0 ? menuStack[menuStack.length - 1] : ({})
    readonly property var currentItems: currentMenu.children ?? []
    readonly property var popupWindow:
        contentItem !== null ? contentItem.Window.window : null
    readonly property string accessibleName:
        qsTr("%1 menu").arg(String(currentMenu.text ?? "Application"))

    function openMenu(menu, anchor) {
        if (menu === null || String(menu.kind ?? "") !== "submenu"
                || !Boolean(menu.enabled) || (menu.children ?? []).length === 0)
            return
        anchorItem = anchor
        menuStack = [menu]
        open()
        Qt.callLater(focusFirstItem)
    }

    function focusFirstItem() {
        popupList.currentIndex = firstEnabledIndex(0, 1)
        popupList.forceActiveFocus(Qt.PopupFocusReason)
    }

    function firstEnabledIndex(start, direction) {
        if (currentItems.length === 0)
            return -1
        let candidate = Math.max(0, Math.min(currentItems.length - 1, start))
        for (let visited = 0; visited < currentItems.length; ++visited) {
            const item = currentItems[candidate]
            if (String(item.kind ?? "") !== "separator" && Boolean(item.enabled))
                return candidate
            candidate = (candidate + direction + currentItems.length)
                    % currentItems.length
        }
        return -1
    }

    function moveCurrent(direction) {
        const start = popupList.currentIndex < 0 ? 0
                                                 : popupList.currentIndex + direction
        popupList.currentIndex = firstEnabledIndex(start, direction)
    }

    function choose(item) {
        if (item === null || !Boolean(item.enabled))
            return
        const kind = String(item.kind ?? "")
        if (kind === "submenu") {
            if (menuStack.length >= Math.max(1, maximumDepth)
                    || (item.children ?? []).length === 0)
                return
            menuStack = menuStack.concat([item])
            Qt.callLater(focusFirstItem)
            return
        }
        if (kind === "action") {
            // AGENT-GUARD: one accepted popup gesture becomes one facade call.
            // The facade/coordinator performs the lineage guard and D-Bus
            // client deliberately does not retry uncertain Event delivery.
            access.activate(String(item.id ?? ""))
            close()
        }
    }

    function back() {
        if (menuStack.length <= 1) {
            close()
            return
        }
        menuStack = menuStack.slice(0, menuStack.length - 1)
        Qt.callLater(focusFirstItem)
    }

    function closeAfterFocusLoss() {
        Qt.callLater(function() {
            if (popup.opened
                    && (!popup.activeFocus
                        || (popup.popupWindow !== null
                            && !popup.popupWindow.active)))
                popup.close()
        })
    }

    objectName: "globalMenuPopup"
    // AGENT-GUARD: The production layer-shell panel deliberately rejects
    // keyboard focus. Keep this as an independent popup window so opening a
    // menu creates the keyboard-capable surface needed by the navigation
    // handlers below; an item popup silently strands focus in RuntimePanel.
    popupType: Popup.Window
    modal: false
    focus: true
    padding: 4
    width: 240
    height: Math.min(360, Math.max(28, popupList.contentHeight + 8))
    x: anchorItem !== null ? anchorItem.x : 0
    y: anchorItem !== null ? anchorItem.y + anchorItem.height : 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                 | Popup.CloseOnPressOutsideParent

    onClosed: {
        menuStack = []
        anchorItem = null
    }
    onActiveFocusChanged: {
        if (opened && !activeFocus)
            closeAfterFocusLoss()
    }

    Connections {
        target: popup.popupWindow

        function onActiveChanged() {
            if (popup.opened && popup.popupWindow !== null
                    && !popup.popupWindow.active)
                popup.closeAfterFocusLoss()
        }
    }

    background: Rectangle {
        radius: popup.theme.cornerRadius ?? 6
        color: popup.colors.surfaceRaised ?? "#2c312e"
        border.color: popup.colors.border ?? "#3c433f"
        border.width: 1
    }

    contentItem: ListView {
        id: popupList

        objectName: "globalMenuPopupList"
        model: popup.currentItems
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationWraps: true
        Accessible.role: Accessible.PopupMenu
        Accessible.name: popup.accessibleName

        Keys.onDownPressed: event => {
            popup.moveCurrent(1)
            event.accepted = true
        }
        Keys.onUpPressed: event => {
            popup.moveCurrent(-1)
            event.accepted = true
        }
        Keys.onRightPressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }
        Keys.onLeftPressed: event => {
            popup.back()
            event.accepted = true
        }
        Keys.onEscapePressed: event => {
            popup.close()
            event.accepted = true
        }
        Keys.onReturnPressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }
        Keys.onEnterPressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }
        Keys.onSpacePressed: event => {
            if (currentIndex >= 0)
                popup.choose(popup.currentItems[currentIndex])
            event.accepted = true
        }

        delegate: Item {
            id: row
            required property var modelData
            required property int index
            readonly property string kind: String(modelData.kind ?? "action")
            readonly property bool separator: kind === "separator"

            objectName: separator ? "globalMenuPopupSeparator"
                                  : "globalMenuPopupItem"
            width: popupList.width
            height: separator ? 9 : 28

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 6
                height: 1
                visible: row.separator
                color: popup.colors.border ?? "#3c433f"
            }

            AbstractButton {
                id: button
                objectName: "globalMenuPopupButton"
                anchors.fill: parent
                visible: !row.separator
                enabled: Boolean(row.modelData.enabled)
                checkable: false
                checked: Boolean(row.modelData.checked ?? false)
                hoverEnabled: true
                Accessible.role: Accessible.MenuItem
                Accessible.focusable: enabled
                Accessible.name: String(row.modelData.text ?? "")
                Accessible.description: row.kind === "submenu"
                    ? qsTr("Opens submenu")
                    : String(row.modelData.shortcutText ?? "")
                Accessible.checkable: Boolean(row.modelData.checkable ?? false)
                Accessible.checked: Boolean(row.modelData.checked ?? false)
                onClicked: popup.choose(row.modelData)
                Accessible.onPressAction: popup.choose(row.modelData)

                background: Rectangle {
                    color: button.hovered || popupList.currentIndex === row.index
                           ? popup.colors.accent ?? "#8fc8b7" : "transparent"
                    radius: 4
                }

                contentItem: Row {
                    spacing: 8
                    Text {
                        width: 18
                        text: Boolean(row.modelData.checkable)
                              ? (Boolean(row.modelData.checked) ? "✓" : "") : ""
                        color: button.hovered || popupList.currentIndex === row.index
                               ? popup.colors.accentText ?? "#10201b"
                               : popup.colors.text ?? "white"
                    }
                    Text {
                        width: Math.max(90, popupList.width - 92)
                        text: String(row.modelData.text ?? "")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: button.enabled
                               ? (button.hovered || popupList.currentIndex === row.index
                                  ? popup.colors.accentText ?? "#10201b"
                                  : popup.colors.text ?? "white")
                               : popup.colors.textMuted ?? "#a9afa9"
                    }
                    Text {
                        width: 42
                        horizontalAlignment: Text.AlignRight
                        text: row.kind === "submenu" ? "›"
                              : String(row.modelData.shortcutText ?? "")
                        textFormat: Text.PlainText
                        color: popup.colors.textMuted ?? "#a9afa9"
                    }
                }
            }
        }
    }
}

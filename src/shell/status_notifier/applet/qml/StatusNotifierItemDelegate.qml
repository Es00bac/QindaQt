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

    // Flattened read-only menu preview rows captured when the context popup
    // opens; dbusmenu entry activation itself is a later composition lane.
    property var menuRows: []

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

    function activate() {
        if (hasAccess && item) {
            access.activateItem(item.uniqueName, item.objectPath, item.generation)
        }
    }

    function openContextPopup() {
        if (!hasAccess || !item)
            return
        menuRows = access.menuRowsFor(item.uniqueName, item.objectPath, item.generation)
        contextPopup.open()
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
        color: delegateRoot.down ? Tokens.state.pressed
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

    // AGENT-GUARD: the pointer area sits BELOW the content item (z: -1) so
    // real input-handling children keep precedence; clipboard's P1 defect was
    // a default-stacked MouseArea swallowing every child click.
    MouseArea {
        id: clickArea
        objectName: "statusNotifierItemClickArea"
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.LeftButton
        onClicked: delegateRoot.activate()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: delegateRoot.openContextPopup()
    }

    T.Popup {
        id: contextPopup
        objectName: "statusNotifierContextPopup"
        padding: Tokens.space["2"]
        // AGENT-GUARD: RuntimePanel rejects focus by design
        // (Qt.WindowDoesNotAcceptFocus), so an item-backed popup strands
        // Escape on the layer-shell dock. This independent popup window is
        // the keyboard-capable surface; do not downgrade it to an item popup.
        popupType: T.Popup.Window
        modal: false
        // Focus lets the popup receive Escape for CloseOnEscape and keeps
        // keyboard dismissal real instead of pointer-only.
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

        // AGENT-NOTE: T.Popup is not an Item, so the Accessible attached
        // property cannot attach to the popup itself (it fatals under
        // QT_FATAL_WARNINGS); the content item carries the menu semantics.
        contentItem: ColumnLayout {
            spacing: Tokens.space["1"]

            Accessible.role: Accessible.PopupMenu
            Accessible.name: delegateRoot.item
                ? qsTr("Context menu for %1").arg(delegateRoot.item.accessibleName)
                : qsTr("Context menu")

            Repeater {
                model: delegateRoot.menuRows

                delegate: Item {
                    required property var modelData
                    objectName: "statusNotifierMenuPreviewRow"

                    readonly property int indent:
                        modelData.depth * Tokens.space["4"]

                    implicitWidth: separator.visible
                        ? 120
                        : previewLabel.implicitWidth + indent
                    implicitHeight: separator.visible
                        ? separator.height + Tokens.space["2"]
                        : previewLabel.implicitHeight

                    Accessible.role: Accessible.StaticText
                    Accessible.name: modelData.kind === "separator" ? "" : modelData.label
                    Accessible.ignored: modelData.kind === "separator"

                    Rectangle {
                        id: separator
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 1
                        color: Tokens.outline.divider
                        visible: modelData.kind === "separator"
                    }

                    C.Label {
                        id: previewLabel
                        anchors.left: parent.left
                        anchors.leftMargin: parent.indent
                        anchors.verticalCenter: parent.verticalCenter
                        visible: modelData.kind !== "separator"
                        enabled: modelData.enabled
                        muted: !modelData.enabled
                        text: modelData.kind === "submenu" && modelData.hasChildren
                              ? modelData.label + " ›"
                              : modelData.label
                    }
                }
            }

            C.Button {
                id: openMenuButton
                objectName: "statusNotifierOpenMenuButton"
                Layout.fillWidth: true
                text: qsTr("Open menu")
                emphasized: false
                available: delegateRoot.hasAccess
                           && delegateRoot.access.activateGranted === true
                accessibleDescription: qsTr("Ask the application to open its context menu")
                onClicked: {
                    if (delegateRoot.hasAccess && delegateRoot.item) {
                        delegateRoot.access.openContextMenu(
                            delegateRoot.item.uniqueName,
                            delegateRoot.item.objectPath,
                            delegateRoot.item.generation)
                    }
                    contextPopup.close()
                }
            }
        }
    }
}

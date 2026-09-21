// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls

Rectangle {
    id: header

    required property var navigation
    // AGENT-GUARD: `required property var navigation` means the host must SET
    // this, not that it is non-null - a `var` is legitimately null while the
    // controller is still being constructed. Reading `navigation.activeRouteId`
    // directly threw "TypeError: Cannot read property 'activeRouteId' of null"
    // 144 times in one live session log. A thrown binding is not a visible
    // crash: it silently evaluates to undefined, so the row highlight and the
    // accessible selected state were simply wrong until something reprojected
    // them. Read through this instead.
    readonly property string activeRouteId:
        navigation ? String(navigation.activeRouteId ?? "") : ""
    signal contentFocusRequested()

    implicitHeight: 48
    color: Tokens.bg.base
    border.width: 1
    border.color: Tokens.outline.divider

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Settings Categories")

    function revealButton(item) {
        if (!item || compactRouteScroller.width <= 0) return

        const left = item.x
        const right = left + item.width
        const viewportLeft = compactRouteScroller.contentX
        const viewportRight = viewportLeft + compactRouteScroller.width
        const maximumX = Math.max(0, compactRouteScroller.contentWidth
                                      - compactRouteScroller.width)

        // AGENT-GUARD: Reveal only on navigation and viewport changes. Binding
        // contentX continuously would fight deliberate touch or pointer scrolling.
        if (left < viewportLeft) {
            compactRouteScroller.contentX = Math.max(0, Math.min(left, maximumX))
        } else if (right > viewportRight) {
            compactRouteScroller.contentX = Math.max(0,
                Math.min(right - compactRouteScroller.width, maximumX))
        }
    }

    function revealActiveButton() {
        if (compactRepeater.count <= 0) return
        const activeIndex = Math.max(0, Math.min(header.navigation.activeIndex,
                                                 compactRepeater.count - 1))
        revealButton(compactRepeater.itemAt(activeIndex))
    }

    function focusActiveButton() {
        if (compactRepeater.count > 0) {
            const activeIndex = Math.max(0, Math.min(header.navigation.activeIndex,
                                                     compactRepeater.count - 1))
            const item = compactRepeater.itemAt(activeIndex)
            if (item) {
                item.forceActiveFocus(Qt.TabFocusReason)
                revealButton(item)
            }
        }
    }

    Component.onCompleted: Qt.callLater(revealActiveButton)

    Connections {
        target: header.navigation
        function onActiveRouteIdChanged() {
            Qt.callLater(header.revealActiveButton)
        }
        function onRoutesChanged() {
            Qt.callLater(header.revealActiveButton)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Tokens.space["3"]
        anchors.rightMargin: Tokens.space["3"]
        spacing: Tokens.space["2"]

        Text {
            text: qsTr("Settings")
            color: Tokens.fg.default
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            font.weight: Font.Bold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Flickable {
            id: compactRouteScroller
            objectName: "settingsCompactRouteScroller"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: compactTabs.width
            contentHeight: compactTabs.height
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick
            onWidthChanged: Qt.callLater(header.revealActiveButton)

            T.ScrollBar.horizontal: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                Accessible.name: qsTr("Scroll settings categories")
            }
            WheelHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => {
                    const pixels = event.pixelDelta.x || event.pixelDelta.y
                    const angle = event.angleDelta.x || event.angleDelta.y
                    const delta = pixels || angle / 120 * compactRouteScroller.height
                    const oldX = compactRouteScroller.contentX
                    const maxX = Math.max(0, compactRouteScroller.contentWidth - compactRouteScroller.width)
                    compactRouteScroller.contentX = Math.max(0, Math.min(maxX, oldX - delta))
                    event.accepted = compactRouteScroller.contentX !== oldX
                }
            }

            Row {
                id: compactTabs
                height: compactRouteScroller.height
                spacing: Tokens.space["1"]

                Repeater {
                    id: compactRepeater
                    model: header.navigation ? header.navigation.routes : []

                    delegate: Controls.Button {
                    id: routeTab
                    property bool routeAvailable: modelData.available

                    objectName: "settingsCompactTab_" + modelData.id
                    text: modelData.title
                    emphasized: header.activeRouteId === modelData.id
                    accessibleDescription: routeAvailable ? modelData.description
                        : qsTr("Unavailable. %1").arg(modelData.unavailableReason)
                    implicitHeight: 32
                    Accessible.role: Accessible.PageTab
                    Accessible.selected: header.activeRouteId === modelData.id

                    onActiveFocusChanged: {
                        if (activeFocus) header.revealButton(routeTab)
                    }

                    onClicked: {
                        if (routeAvailable) {
                            header.navigation.selectRoute(modelData.id);
                        }
                    }

                    Keys.onLeftPressed: {
                        if (index > 0) {
                            const prevItem = compactRepeater.itemAt(index - 1);
                            if (prevItem) prevItem.forceActiveFocus(Qt.TabFocusReason);
                        }
                    }

                    Keys.onRightPressed: {
                        if (index < compactRepeater.count - 1) {
                            const nextItem = compactRepeater.itemAt(index + 1);
                            if (nextItem) nextItem.forceActiveFocus(Qt.TabFocusReason);
                        }
                    }

                    Keys.onTabPressed: event => {
                        header.contentFocusRequested()
                        event.accepted = true
                    }
                    }
                }
            }
        }
    }
}

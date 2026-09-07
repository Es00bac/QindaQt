// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls

Rectangle {
    id: header

    required property var navigation
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

        Item {
            Layout.fillWidth: true
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
            onWidthChanged: Qt.callLater(header.revealActiveButton)

            Row {
                id: compactTabs
                height: compactRouteScroller.height
                spacing: Tokens.space["1"]

                Repeater {
                    id: compactRepeater
                    model: header.navigation.routes

                    delegate: Controls.Button {
                    id: routeTab
                    property bool routeAvailable: modelData.available

                    objectName: "settingsCompactTab_" + modelData.id
                    text: modelData.title
                    emphasized: header.navigation.activeRouteId === modelData.id
                    accessibleDescription: routeAvailable ? modelData.description
                        : qsTr("Unavailable. %1").arg(modelData.unavailableReason)
                    implicitHeight: 32
                    Accessible.role: Accessible.PageTab
                    Accessible.selected: header.navigation.activeRouteId === modelData.id

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

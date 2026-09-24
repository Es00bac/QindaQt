// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls

Rectangle {
    id: sidebar

    required property var navigation
    // AGENT-GUARD: `required property var navigation` means the host must SET
    // this, not that it is non-null - a `var` is legitimately null while the
    // controller is still being constructed. Reading `navigation.activeRouteId`
    // directly threw "TypeError: Cannot read property 'activeRouteId' of null"
    // 72 times in one live session log. A thrown binding is not a visible
    // crash: it silently evaluates to undefined, so the row highlight and the
    // accessible selected state were simply wrong until something reprojected
    // them. Read through this instead.
    readonly property string activeRouteId:
        navigation ? String(navigation.activeRouteId ?? "") : ""
    signal contentFocusRequested()
    // The host owns the search palette (ADR-0257); this only asks for it.
    signal searchRequested()

    implicitWidth: 224
    color: Tokens.bg.base
    border.width: 1
    border.color: Tokens.outline.divider

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Settings Navigation")

    // The controller retains insertion order for shortcuts and history. This
    // local ordering only gives the desktop sidebar readable categories.
    readonly property var orderedRoutes: {
        // An empty list, not a bare `return`: consumers iterate this, and
        // undefined would trade a binding error for a worse one.
        if (!sidebar.navigation)
            return []
        const routes = sidebar.navigation.routes.slice()
        const rank = category => category === "General" ? 0
            : category === "Personalization" ? 1 : 2
        routes.sort((left, right) => {
            const categoryDifference = rank(left.category) - rank(right.category)
            return categoryDifference !== 0 ? categoryDifference
                                            : left.title.localeCompare(right.title)
        })
        return routes
    }

    function focusActiveButton() {
        for (let index = 0; index < navRepeater.count; ++index) {
            const button = navRepeater.itemAt(index)
            if (button !== null && button.routeId === sidebar.activeRouteId) {
                button.routeButton.forceActiveFocus(Qt.TabFocusReason)
                revealButton(button.routeButton)
                return
            }
        }
    }

    function revealButton(button) {
        if (!button || navScroll.height <= 0)
            return
        const top = button.mapToItem(navButtonsColumn, 0, 0).y
        const bottom = top + button.height
        const maximumY = Math.max(0, navScroll.contentHeight - navScroll.height)
        if (top < navScroll.contentY)
            navScroll.contentY = Math.max(0, Math.min(top, maximumY))
        else if (bottom > navScroll.contentY + navScroll.height)
            navScroll.contentY = Math.max(0, Math.min(bottom - navScroll.height, maximumY))
    }

    function revealActiveButton() {
        for (let index = 0; index < navRepeater.count; ++index) {
            const row = navRepeater.itemAt(index)
            if (row && row.routeId === sidebar.activeRouteId) {
                revealButton(row.routeButton)
                return
            }
        }
    }

    Component.onCompleted: Qt.callLater(revealActiveButton)
    onOrderedRoutesChanged: Qt.callLater(revealActiveButton)
    onVisibleChanged: if (visible) Qt.callLater(revealActiveButton)
    Connections {
        target: sidebar.navigation
        function onActiveRouteIdChanged() { Qt.callLater(sidebar.revealActiveButton) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["3"]
        spacing: Tokens.space["2"]

        Text {
            objectName: "settingsSidebarTitle"
            text: qsTr("Settings")
            // This labels the navigation region. Route pages own the single
            // prominent title, so the sidebar label stays a quiet app cue.
            color: Tokens.fg.muted
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.subtitle
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            Layout.topMargin: Tokens.space["1"]
            Layout.bottomMargin: Tokens.space["1"]
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Controls.Button {
            objectName: "settingsSidebarSearchButton"
            Layout.fillWidth: true
            emphasized: false
            text: qsTr("Search settings")
            accessibleDescription: qsTr("Find a settings page by name or topic. Shortcut: Ctrl+K")
            onClicked: sidebar.searchRequested()
        }

        Flickable {
            id: navScroll
            objectName: "settingsSidebarScroller"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: navButtonsColumn.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            onHeightChanged: Qt.callLater(sidebar.revealActiveButton)

            T.ScrollBar.vertical: T.ScrollBar {
                id: navScrollBar
                objectName: "settingsSidebarScrollBar"
                policy: T.ScrollBar.AsNeeded
                Accessible.name: qsTr("Scroll settings categories")
            }

            // AGENT-GUARD: Only the viewport fills the available height. The
            // full route column keeps its natural height, including headings.
            ColumnLayout {
                id: navButtonsColumn
                width: Math.max(0, navScroll.width - navScrollBar.width - Tokens.space["1"])
                spacing: Tokens.space["1"]

                Repeater {
                    id: navRepeater
                    model: sidebar.orderedRoutes

                    delegate: ColumnLayout {
                        id: navRow
                        required property var modelData
                        required property int index
                        property string routeId: modelData.id
                        property alias routeButton: navBtn
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        Text {
                            objectName: "settingsSidebarCategory_" + modelData.category
                            Layout.fillWidth: true
                            visible: index === 0
                                     || modelData.category !== sidebar.orderedRoutes[index - 1].category
                            Layout.topMargin: index === 0 ? 0 : Tokens.space["2"]
                            text: modelData.category
                            color: Tokens.fg.muted
                            font.family: Tokens.type.fontFamily
                            font.pointSize: Tokens.type.caption
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsNavButton {
                            id: navBtn
                            objectName: "settingsNavButton_" + modelData.id
                            Layout.fillWidth: true
                            text: modelData.title
                            routeId: modelData.id
                            iconName: modelData.iconName ?? ""
                            category: ""
                            routeDescription: modelData.description
                            unavailableReason: modelData.unavailableReason
                            active: sidebar.activeRouteId === modelData.id
                            routeAvailable: modelData.available
                            onActiveFocusChanged: if (activeFocus) sidebar.revealButton(navBtn)

                            onClicked: {
                                if (modelData.available)
                                    sidebar.navigation.selectRoute(modelData.id)
                            }

                            Keys.onUpPressed: {
                                if (index > 0) {
                                    const previous = navRepeater.itemAt(index - 1);
                                    if (previous) previous.routeButton.forceActiveFocus(Qt.TabFocusReason);
                                }
                            }

                            Keys.onDownPressed: {
                                if (index < navRepeater.count - 1) {
                                    const next = navRepeater.itemAt(index + 1);
                                    if (next) next.routeButton.forceActiveFocus(Qt.TabFocusReason);
                                }
                            }

                            Keys.onTabPressed: event => {
                                sidebar.contentFocusRequested()
                                event.accepted = true
                            }
                        }
                    }
                }
            }
        }
    }
}

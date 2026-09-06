// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

Rectangle {
    id: sidebar

    required property var navigation
    signal contentFocusRequested()

    implicitWidth: 224
    color: Tokens.bg.base
    border.width: 1
    border.color: Tokens.outline.divider

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Settings Navigation")

    // The controller retains insertion order for shortcuts and history. This
    // local ordering only gives the desktop sidebar readable categories.
    readonly property var orderedRoutes: {
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
            if (button !== null && button.routeId === sidebar.navigation.activeRouteId) {
                button.routeButton.forceActiveFocus(Qt.TabFocusReason)
                return
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["3"]
        spacing: Tokens.space["2"]

        Text {
            objectName: "settingsSidebarTitle"
            text: qsTr("Settings")
            color: Tokens.fg.default
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.Bold
            Layout.fillWidth: true
            Layout.topMargin: Tokens.space["2"]
            Layout.bottomMargin: Tokens.space["2"]
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        ColumnLayout {
            id: navButtonsColumn
            Layout.fillWidth: true
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
                        category: ""
                        routeDescription: modelData.description
                        unavailableReason: modelData.unavailableReason
                        active: sidebar.navigation.activeRouteId === modelData.id
                        routeAvailable: modelData.available

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

        Item {
            Layout.fillHeight: true
        }
    }
}

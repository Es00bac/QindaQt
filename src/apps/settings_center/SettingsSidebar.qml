// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

Rectangle {
    id: sidebar

    required property var navigation
    signal contentFocusRequested()

    implicitWidth: 200
    color: Tokens.bg.base
    border.width: 1
    border.color: Tokens.outline.divider

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Settings Navigation")

    function focusActiveButton() {
        if (navRepeater.count > 0) {
            const activeIdx = Math.max(0, sidebar.navigation.activeIndex);
            const item = navRepeater.itemAt(activeIdx);
            if (item) {
                item.forceActiveFocus(Qt.TabFocusReason);
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
                model: sidebar.navigation.routes

                delegate: SettingsNavButton {
                    id: navBtn
                    objectName: "settingsNavButton_" + modelData.id
                    Layout.fillWidth: true
                    text: modelData.title
                    routeId: modelData.id
                    category: modelData.category
                    routeDescription: modelData.description
                    unavailableReason: modelData.unavailableReason
                    active: sidebar.navigation.activeRouteId === modelData.id
                    routeAvailable: modelData.available

                    onClicked: {
                        if (modelData.available) {
                            sidebar.navigation.selectRoute(modelData.id);
                        }
                    }

                    Keys.onUpPressed: {
                        if (index > 0) {
                            const prevItem = navRepeater.itemAt(index - 1);
                            if (prevItem) prevItem.forceActiveFocus(Qt.TabFocusReason);
                        }
                    }

                    Keys.onDownPressed: {
                        if (index < navRepeater.count - 1) {
                            const nextItem = navRepeater.itemAt(index + 1);
                            if (nextItem) nextItem.forceActiveFocus(Qt.TabFocusReason);
                        }
                    }

                    Keys.onTabPressed: event => {
                        sidebar.contentFocusRequested()
                        event.accepted = true
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}

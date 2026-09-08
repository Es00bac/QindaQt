// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Item {
    id: root
    required property var navigationController
    readonly property int segmentLimit: width < 350 ? 2 : 3
    readonly property var visibleSegments: navigationController.breadcrumb.slice(-segmentLimit)
    readonly property var hiddenSegments: navigationController.breadcrumb.slice(0,
        Math.max(0, navigationController.breadcrumb.length - segmentLimit))
    implicitHeight: 40

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Current folder: %1").arg(navigationController.currentPath)

    RowLayout {
        anchors.fill: parent
        spacing: Tokens.space["1"]
        IconButton {
            objectName: "breadcrumbAncestorsButton"
            Layout.preferredWidth: 32
            visible: root.hiddenSegments.length > 0
            iconName: "view-more"
            text: qsTr("Parent folders")
            onClicked: ancestors.popup()
            T.Menu {
                id: ancestors
                Instantiator {
                    model: root.hiddenSegments
                    delegate: T.MenuItem {
                        required property var modelData
                        text: modelData.name
                        Accessible.description: modelData.path
                        onTriggered: root.navigationController.navigateTo(modelData.path)
                    }
                    onObjectAdded: (index, object) => ancestors.insertItem(index, object)
                    onObjectRemoved: (index, object) => ancestors.removeItem(object)
                }
            }
        }
        Repeater {
            model: root.visibleSegments
            Qinda.Button {
                id: segment
                required property var modelData
                required property int index
                objectName: "breadcrumbSegment_" + (root.hiddenSegments.length + index)
                Layout.fillWidth: true
                Layout.maximumWidth: 180
                Layout.minimumWidth: 24
                implicitWidth: Math.min(180, implicitContentWidth + 16)
                leftPadding: Tokens.space["2"]
                rightPadding: Tokens.space["2"]
                text: modelData.name
                emphasized: false
                accessibleDescription: qsTr("Open %1").arg(modelData.path)
                contentItem: Qinda.Label {
                    text: segment.text
                    elide: Text.ElideMiddle
                    wrapMode: Text.NoWrap
                    maximumLineCount: 1
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    Accessible.ignored: true
                }
                background: Rectangle {
                    radius: Tokens.radius.m
                    color: segment.down ? Tokens.state.pressed : segment.hovered ? Tokens.state.hover
                         : segment.index === root.visibleSegments.length - 1 ? Tokens.state.pressed : "transparent"
                    border.width: segment.activeFocus ? 2 : 0
                    border.color: Tokens.focus.ring
                }
                onClicked: root.navigationController.navigateTo(modelData.path)
            }
        }
    }
}

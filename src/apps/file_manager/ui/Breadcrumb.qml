// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root
    required property var navigationController
    readonly property int segmentLimit: width < 350 ? 2 : 3
    readonly property var visibleSegments: navigationController.breadcrumb.slice(-segmentLimit)
    readonly property var hiddenSegments: navigationController.breadcrumb.slice(0,
        Math.max(0, navigationController.breadcrumb.length - segmentLimit))
    implicitHeight: 40
    padding: 0

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Current folder: %1").arg(navigationController.currentPath)

    contentItem: RowLayout {
        spacing: 4
        IconButton {
            objectName: "breadcrumbAncestorsButton"
            Layout.preferredWidth: 32
            visible: root.hiddenSegments.length > 0
            iconName: "view-more"
            text: qsTr("Parent folders")
            onClicked: ancestors.popup()
            Menu {
                id: ancestors
                Instantiator {
                    model: root.hiddenSegments
                    delegate: MenuItem {
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
            Button {
                id: segment
                required property var modelData
                required property int index
                objectName: "breadcrumbSegment_" + (root.hiddenSegments.length + index)
                Layout.fillWidth: true
                Layout.maximumWidth: 180
                Layout.minimumWidth: 24
                implicitWidth: Math.min(180, implicitContentWidth + 16)
                flat: true
                text: modelData.name
                Accessible.description: qsTr("Open %1").arg(modelData.path)
                contentItem: Label {
                    text: segment.text
                    elide: Text.ElideMiddle
                    wrapMode: Text.NoWrap
                    maximumLineCount: 1
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    Accessible.ignored: true
                }
                background: Rectangle {
                    radius: 6
                    color: segment.down ? segment.palette.mid
                         : segment.hovered ? segment.palette.alternateBase
                         : segment.index === root.visibleSegments.length - 1
                           ? segment.palette.alternateBase : "transparent"
                    border.width: segment.activeFocus ? 2 : 0
                    border.color: segment.palette.highlight
                }
                onClicked: root.navigationController.navigateTo(modelData.path)
            }
        }
    }
}

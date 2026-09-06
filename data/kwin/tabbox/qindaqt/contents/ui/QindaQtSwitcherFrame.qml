// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: frame

    required property var nativeModel
    required property rect screenGeometry
    property alias currentIndex: windowList.currentIndex
    signal activateRequested(int index)

    readonly property int rowHeight: 58
    readonly property int visibleRows: Math.max(1, Math.min(7, windowList.count))

    function select(index) {
        windowList.currentIndex = index
        windowList.positionViewAtIndex(index, ListView.Contain)
    }

    width: Math.min(620, Math.max(360, screenGeometry.width * 0.42))
    height: heading.implicitHeight + windowList.height
            + Kirigami.Units.largeSpacing * 3
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.backgroundColor
    border.width: 1
    border.color: Kirigami.Theme.disabledTextColor

    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Switch windows and groups")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        Text {
            id: heading

            Layout.fillWidth: true
            text: qsTr("Switch windows and groups")
            color: Kirigami.Theme.textColor
            font.family: Kirigami.Theme.defaultFont.family
            font.weight: Font.DemiBold
            font.pointSize: Kirigami.Theme.defaultFont.pointSize + 1
            elide: Text.ElideRight
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
        }

        ListView {
            id: windowList

            Layout.fillWidth: true
            Layout.preferredHeight: frame.rowHeight * frame.visibleRows
            clip: true
            focus: true
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            highlightResizeDuration: 0
            model: frame.nativeModel

            delegate: QindaQtSwitcherRow {
                width: windowList.width
                height: frame.rowHeight
                selected: index === windowList.currentIndex
                onTriggered: {
                    windowList.currentIndex = index
                    frame.activateRequested(index)
                }
            }

            Text {
                anchors.centerIn: parent
                visible: windowList.count === 0
                text: qsTr("No open windows")
                color: Kirigami.Theme.disabledTextColor
                font.family: Kirigami.Theme.defaultFont.family
                font.pointSize: Kirigami.Theme.defaultFont.pointSize
                textFormat: Text.PlainText
                Accessible.role: Accessible.StaticText
            }
        }

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) {
                if (windowList.count > 0) {
                    windowList.currentIndex = windowList.currentIndex === 0
                        ? windowList.count - 1 : windowList.currentIndex - 1
                }
                event.accepted = true
            } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down) {
                if (windowList.count > 0) {
                    windowList.currentIndex =
                        (windowList.currentIndex + 1) % windowList.count
                }
                event.accepted = true
            }
        }
    }
}

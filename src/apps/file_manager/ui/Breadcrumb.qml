// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Rectangle {
    id: root

    required property var navigationController

    implicitHeight: flow.implicitHeight + Tokens.space["2"] * 2
    color: Tokens.bg.base

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Current folder: %1").arg(navigationController.currentPath)

    T.ScrollView {
        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        contentWidth: flow.implicitWidth

        Row {
            id: flow
            spacing: Tokens.space["1"]

            Repeater {
                model: root.navigationController.breadcrumb

                Qinda.Button {
                    required property var modelData
                    required property int index

                    objectName: "breadcrumbSegment_" + index
                    text: modelData.name
                    emphasized: index === root.navigationController.breadcrumb.length - 1
                    accessibleDescription: qsTr("Open %1").arg(modelData.path)
                    onClicked: root.navigationController.navigateTo(modelData.path)
                }
            }
        }
    }
}

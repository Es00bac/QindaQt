// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    width: 390
    height: 78
    radius: themeCatalog.current.cornerRadius ?? 10
    color: themeCatalog.current.colors?.surfaceRaised ?? "#2c312e"
    border.color: themeCatalog.current.colors?.border ?? "#3c433f"
    opacity: 0.97

    Row {
        anchors.centerIn: parent
        spacing: 10

        Column {
            spacing: 3
            Text {
                text: qsTr("Desktop profile")
                color: themeCatalog.current.colors?.textMuted ?? "#a9afa9"
                font.pixelSize: 10
            }
            ComboBox {
                width: 190
                model: profileCatalog.items
                textRole: "name"
                currentIndex: profileCatalog.currentIndex
                onActivated: profileCatalog.selectIndex(currentIndex)
            }
        }

        Column {
            spacing: 3
            Text {
                text: qsTr("Theme")
                color: themeCatalog.current.colors?.textMuted ?? "#a9afa9"
                font.pixelSize: 10
            }
            ComboBox {
                width: 150
                model: themeCatalog.items
                textRole: "name"
                currentIndex: themeCatalog.currentIndex
                onActivated: themeCatalog.selectIndex(currentIndex)
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0

Rectangle {
    id: diagram
    required property string kind
    implicitHeight: 184
    radius: Tokens.radius.l
    color: Tokens.bg.base
    border.width: 1
    border.color: Tokens.outline.divider
    clip: true
    Accessible.role: Accessible.Graphic
    Accessible.name: kind === "desktop" ? qsTr("Desktop with a top panel and dock")
                     : kind === "dock" ? qsTr("Window dragged toward split and page targets")
                     : kind === "container" ? qsTr("A group with two pages and a nested split")
                     : kind === "move" ? qsTr("Outer and member title controls")
                     : kind === "customize" ? qsTr("Preset and Customize workflow")
                     : qsTr("Appearance preview")

    component MiniWindow: Rectangle {
        property string label: ""
        property bool accented: false
        radius: Tokens.radius.m
        color: accented ? Tokens.accent.subtle : Tokens.bg.raised
        border.width: 1
        border.color: accented ? Tokens.accent.default : Tokens.outline.strong
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 24; radius: parent.radius
            color: parent.accented ? Tokens.accent.default : Tokens.bg.highest
        }
        Text {
            anchors.centerIn: parent; anchors.verticalCenterOffset: 10
            text: parent.label; color: Tokens.fg.default
            font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption
        }
    }

    Item {
        anchors.fill: parent; anchors.margins: Tokens.space["5"]
        visible: diagram.kind === "desktop"
        Rectangle { anchors.fill: parent; radius: Tokens.radius.m; color: Tokens.accent.subtle; border.width: 1; border.color: Tokens.outline.divider }
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 22; radius: Tokens.radius.s; color: Tokens.bg.highest
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Applications   File   Edit   View"); color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 8; spacing: 7
            Repeater { model: 6; Rectangle { width: 28; height: 28; radius: 8; color: index === 1 ? Tokens.accent.default : Tokens.bg.highest; border.width: 1; border.color: Tokens.outline.strong } }
        }
        MiniWindow { x: 42; y: 42; width: 145; height: 86; label: qsTr("Your work"); accented: true }
    }

    Item {
        anchors.fill: parent; anchors.margins: Tokens.space["5"]
        visible: diagram.kind === "dock"
        MiniWindow { x: 14; y: 30; width: 142; height: 106; label: qsTr("Drag me"); accented: true }
        Rectangle {
            x: parent.width - 126; y: 12; width: 112; height: 132; radius: Tokens.radius.m
            color: Tokens.accent.subtle; border.width: 2; border.color: Tokens.accent.default
            Text { anchors.centerIn: parent; text: qsTr("EDGE\nSplit"); horizontalAlignment: Text.AlignHCenter; color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.body; font.weight: Font.DemiBold }
        }
        Rectangle {
            x: parent.width / 2 - 36; y: parent.height / 2 - 27; width: 72; height: 54; radius: Tokens.radius.m
            color: Tokens.bg.highest; border.width: 2; border.color: Tokens.accent.default
            Text { anchors.centerIn: parent; text: qsTr("CENTER\nPage"); horizontalAlignment: Text.AlignHCenter; color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; text: qsTr("Meta + Shift + left drag"); color: Tokens.accent.default; font.family: Tokens.type.monoFontFamily; font.pointSize: Tokens.type.caption; font.weight: Font.DemiBold }
    }

    Item {
        anchors.fill: parent; anchors.margins: Tokens.space["5"]
        visible: diagram.kind === "container"
        Rectangle { anchors.fill: parent; radius: Tokens.radius.m; color: Tokens.bg.raised; border.width: 2; border.color: Tokens.outline.strong }
        Row {
            x: 10; y: 8; spacing: 4
            Rectangle { width: 100; height: 28; radius: Tokens.radius.s; color: Tokens.accent.default; Text { anchors.centerIn: parent; text: qsTr("Research"); color: Tokens.accent.fg; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption } }
            Rectangle { width: 100; height: 28; radius: Tokens.radius.s; color: Tokens.bg.highest; Text { anchors.centerIn: parent; text: qsTr("Writing"); color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption } }
        }
        MiniWindow { x: 10; y: 45; width: parent.width * 0.48 - 14; height: parent.height - 55; label: qsTr("Browser") }
        MiniWindow { x: parent.width * 0.5 + 3; y: 45; width: parent.width * 0.5 - 13; height: (parent.height - 60) * 0.48; label: qsTr("Notes"); accented: true }
        MiniWindow { x: parent.width * 0.5 + 3; y: parent.height * 0.56; width: parent.width * 0.5 - 13; height: parent.height * 0.44 - 10; label: qsTr("Terminal") }
    }

    Item {
        anchors.fill: parent; anchors.margins: Tokens.space["5"]
        visible: diagram.kind === "move"
        Rectangle { x: 18; y: 12; width: parent.width - 36; height: parent.height - 24; radius: Tokens.radius.m; color: Tokens.bg.raised; border.width: 2; border.color: Tokens.outline.strong }
        Rectangle {
            x: 28; y: 24; width: parent.width - 56; height: 30; radius: Tokens.radius.s; color: Tokens.accent.default
            Text { anchors.centerIn: parent; text: qsTr("Outer title — move the whole group"); color: Tokens.accent.fg; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption }
        }
        MiniWindow { x: 30; y: 66; width: parent.width * 0.47 - 32; height: parent.height - 100; label: qsTr("Member title") }
        MiniWindow { x: parent.width * 0.5 + 4; y: 66; width: parent.width * 0.47 - 20; height: parent.height - 100; label: qsTr("Member title"); accented: true }
    }

    Item {
        anchors.fill: parent; anchors.margins: Tokens.space["5"]
        visible: diagram.kind === "customize"
        RowLayout {
            anchors.centerIn: parent; spacing: Tokens.space["4"]
            Repeater {
                model: [qsTr("Choose a\npreset"), qsTr("Customize\npanels & applets"), qsTr("Apply")]
                RowLayout {
                    id: customizeStep
                    required property string modelData
                    required property int index
                    Rectangle {
                        Layout.preferredWidth: index === 1 ? 150 : 112; Layout.preferredHeight: index === 1 ? 110 : 82
                        radius: Tokens.radius.m; color: index === 1 ? Tokens.accent.subtle : index === 2 ? Tokens.accent.default : Tokens.bg.raised
                        border.width: index === 1 ? 2 : 1; border.color: index === 1 ? Tokens.accent.default : Tokens.outline.strong
                        Text { anchors.centerIn: parent; text: customizeStep.modelData; horizontalAlignment: Text.AlignHCenter; color: customizeStep.index === 2 ? Tokens.accent.fg : Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.body; font.weight: Font.DemiBold }
                    }
                    Text { visible: index < 2; text: "→"; color: Tokens.accent.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.headline }
                }
            }
        }
    }

    Item {
        anchors.fill: parent; anchors.margins: Tokens.space["5"]
        visible: diagram.kind === "appearance"
        RowLayout {
            anchors.fill: parent; spacing: Tokens.space["4"]
            Repeater {
                model: [qsTr("Current appearance"), qsTr("Readable & comfortable")]
                Rectangle {
                    id: appearancePreview
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true; Layout.fillHeight: true; radius: Tokens.radius.m
                    color: index === 0 ? Tokens.bg.highest : Tokens.accent.subtle
                    border.width: 1; border.color: index === 0 ? Tokens.outline.strong : Tokens.accent.default
                    Column { anchors.centerIn: parent; spacing: 8
                        Rectangle { width: 78; height: 34; radius: Tokens.radius.m; color: index === 0 ? Tokens.bg.raised : Tokens.accent.default }
                        Rectangle { width: 92; height: 10; radius: 5; color: Tokens.fg.default }
                        Rectangle { width: 72; height: 8; radius: 4; color: Tokens.fg.muted }
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 9; text: appearancePreview.modelData; color: Tokens.fg.default; font.family: Tokens.type.fontFamily; font.pointSize: Tokens.type.caption }
                }
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QindaQt.Shell

Item {
    id: root

    required property var profile
    required property var theme
    readonly property var colors: theme.colors ?? ({})

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.lighter(root.colors.canvas ?? "#171a18", 1.14) }
            GradientStop { position: 0.62; color: root.colors.canvas ?? "#171a18" }
            GradientStop { position: 1.0; color: Qt.darker(root.colors.canvas ?? "#171a18", 1.18) }
        }
    }

    Rectangle {
        width: parent.width * 0.46
        height: width
        radius: width / 2
        x: parent.width * 0.38
        y: parent.height * 0.16
        color: root.colors.accent ?? "#8fc8b7"
        opacity: 0.055
    }

    WindowGroupPreview {
        width: Math.min(parent.width * 0.72, 980)
        height: Math.min(parent.height * 0.58, 580)
        anchors.centerIn: parent
        theme: root.theme
        profile: root.profile
    }

    Repeater {
        model: root.profile.panels ?? []

        PanelSurface {
            required property var modelData
            panel: modelData
            theme: root.theme
            desktopWidth: root.width
            desktopHeight: root.height
        }
    }

    Rectangle {
        visible: root.profile.workflow?.overview === "activities"
        anchors.fill: parent
        anchors.margins: 72
        radius: root.theme.cornerRadius ?? 10
        color: root.colors.surface ?? "#222624"
        border.color: root.colors.border ?? "#3c433f"
        opacity: 0.92
        z: 8

        Text {
            anchors.centerIn: parent
            text: qsTr("Activities overview · Search applications and workspaces")
            color: root.colors.text ?? "white"
            font.pixelSize: 22
        }
    }
}

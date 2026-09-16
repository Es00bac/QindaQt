// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One output bus (ADR-0173): the destination every strip's matrix column can
// send into. Physical buses drive real devices; virtual buses are sinks other
// applications record from, which is how a streamer captures a submix.
ColumnLayout {
    id: root

    required property var model
    required property var bus
    required property bool enabledControls

    spacing: Tokens.space["2"]
    objectName: "consoleBus_" + bus.id

    Label {
        Layout.fillWidth: true
        text: root.bus.label
        horizontalAlignment: Text.AlignHCenter
        opacity: root.bus.bound ? 1.0 : 0.55
        Accessible.name: root.bus.bound
            ? qsTr("Bus %1").arg(text)
            : qsTr("Bus %1, no device connected").arg(text)
    }

    Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: root.bus.virtual ? qsTr("virtual") : qsTr("physical")
        font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })
        opacity: 0.7
        Accessible.ignored: true
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: Tokens.space["1"]

        T.Slider {
            id: fader
            objectName: "consoleBusFader_" + root.bus.id
            orientation: Qt.Vertical
            implicitHeight: 160
            from: 0.0
            to: 1.0
            enabled: root.enabledControls
            value: root.bus.faderPosition
            onMoved: root.model.setBusFader(root.bus.id, value)
            Accessible.name: qsTr("Bus %1 level").arg(root.bus.label)
            Accessible.description: qsTr("%1 decibels")
                .arg(Math.round(root.model.gainForFaderPosition(value) * 10) / 10)

            background: Rectangle {
                x: fader.leftPadding + fader.availableWidth / 2 - width / 2
                y: fader.topPadding
                implicitWidth: 6
                width: implicitWidth
                height: fader.availableHeight
                radius: 3
                color: Tokens.bg.raised
                Rectangle {
                    width: 14
                    height: 2
                    x: -4
                    y: fader.availableHeight * (1.0 - root.model.unityFaderPosition())
                    color: Tokens.outline.strong
                }
            }
        }

        Rectangle {
            objectName: "consoleBusMeter_" + root.bus.id
            implicitWidth: 8
            implicitHeight: 160
            radius: 4
            color: Tokens.bg.raised
            Rectangle {
                width: parent.width
                radius: parent.radius
                anchors.bottom: parent.bottom
                color: Tokens.accent.default
                visible: root.bus.level.known
                height: parent.height * Math.max(0.0, Math.min(1.0,
                    (root.bus.level.peakDb + 60.0) / 60.0))
            }
        }
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("%1 dB").arg(Math.round(root.bus.gainDb * 10) / 10)
        font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })
        Accessible.ignored: true
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: Tokens.space["1"]

        Button {
            objectName: "consoleBusMute_" + root.bus.id
            text: qsTr("M")
            checkable: true
            checked: root.bus.muted
            enabled: root.enabledControls
            onToggled: root.model.setBusMuted(root.bus.id, checked)
            Accessible.name: qsTr("Mute bus %1").arg(root.bus.label)
        }
        Button {
            objectName: "consoleBusMono_" + root.bus.id
            text: qsTr("Mono")
            checkable: true
            checked: root.bus.mono
            enabled: root.enabledControls
            onToggled: root.model.setBusMono(root.bus.id, checked)
            Accessible.name: qsTr("Mono bus %1").arg(root.bus.label)
        }
    }
}

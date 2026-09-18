// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The foot of the panel: one status line for every condition that used to own
// a card of its own, the overflow counts, and — only when something has
// actually handed the applet a way to do it — a way into Settings.
ColumnLayout {
    id: footer

    required property var controller
    required property var desktopControls
    required property string statusText
    required property bool statusIsError
    required property bool showLists
    signal dismissRequested()

    readonly property int overflowDevices: controller?.overflowDeviceCount ?? 0
    readonly property int overflowStreams: controller?.overflowStreamCount ?? 0

    spacing: Tokens.space["1"]

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Tokens.outline.divider
    }

    C.Label {
        objectName: "audioOverflowSummary"
        Layout.fillWidth: true
        visible: footer.showLists
                 && (footer.overflowDevices > 0 || footer.overflowStreams > 0)
        text: {
            if (footer.overflowDevices > 0 && footer.overflowStreams > 0) {
                return qsTr("%1 more devices and %2 more streams are in Settings.")
                    .arg(footer.overflowDevices).arg(footer.overflowStreams)
            }
            if (footer.overflowDevices > 0) {
                return qsTr("%1 more devices are in Settings.")
                    .arg(footer.overflowDevices)
            }
            return qsTr("%1 more streams are in Settings.")
                .arg(footer.overflowStreams)
        }
        font: Qt.font({ family: Tokens.type.fontFamily,
                        pointSize: Tokens.type.caption })
        muted: true
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]
        visible: footer.statusText.length > 0

        Rectangle {
            Layout.preferredWidth: 3
            Layout.fillHeight: true
            Layout.minimumHeight: 16
            radius: 1
            color: footer.statusIsError ? Tokens.danger.default
                                        : Tokens.status.warning.background
        }

        C.Label {
            objectName: "audioStatusLine"
            Layout.fillWidth: true
            text: footer.statusText
            wrapMode: Text.WordWrap
            font: Qt.font({ family: Tokens.type.fontFamily,
                            pointSize: Tokens.type.caption })
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        AudioAppletPad {
            objectName: "audioStatusDismiss"
            visible: footer.controller?.feedbackPresent ?? false
            text: qsTr("Dismiss")
            accessibleDescription: qsTr("Dismisses the audio change message")
            onClicked: footer.dismissRequested()
        }
    }

    // AGENT-GUARD: rendered only when a composition has supplied something
    // that can actually open Settings. Nothing does today — see the
    // `desktopControls` note in AudioApplet.qml — so no dead affordance is
    // ever shown.
    AudioAppletPad {
        objectName: "audioOpenSettings"
        Layout.alignment: Qt.AlignRight
        visible: footer.desktopControls !== null
                 && Boolean(footer.desktopControls?.canOpenSettings)
        text: qsTr("Audio settings…")
        accessibleDescription: qsTr("Opens the Audio page of QindaQt Settings")
        onClicked: footer.desktopControls.openSettings()
    }
}

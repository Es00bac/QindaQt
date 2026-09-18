// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Network hub's "Nearby" list (ADR-0197). Presentation only: opening a
// row hands its address to NavigationController exactly as a saved location
// does, so an unreachable server lands on the ordinary navigation state pane.
//
// The whole section is hidden when no discovery backend was injected, so a
// platform without Avahi shows a hub that simply has no Nearby part rather
// than an empty list that looks broken.
ColumnLayout {
    id: root
    objectName: "nearbyServersSection"

    required property var discoveryController

    signal openRequested(string url)
    signal saveRequested(string url)

    visible: root.discoveryController.supported
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Nearby")
            font.bold: true
            Accessible.ignored: true
        }
        BusyIndicator {
            objectName: "nearbyBusyIndicator"
            running: root.discoveryController.scanning
                     && root.discoveryController.services.length === 0
            visible: running
            implicitWidth: 20
            implicitHeight: 20
        }
    }

    Label {
        objectName: "nearbyDisabledNotice"
        Layout.fillWidth: true
        visible: !root.discoveryController.scanning
        text: qsTr("Looking for nearby servers is off. Turn it on in Preferences "
                 + "(Network) to see machines that advertise SFTP or Windows sharing "
                 + "on this network.")
        color: root.palette.placeholderText
        wrapMode: Text.WordWrap
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    StatusBanner {
        objectName: "nearbyUnavailableBanner"
        Layout.fillWidth: true
        visible: root.discoveryController.unavailableReason.length > 0
        title: qsTr("Nearby servers")
        message: root.discoveryController.unavailableReason
        actionText: qsTr("Dismiss")
        onActionTriggered: root.discoveryController.clearUnavailableReason()
    }

    Label {
        objectName: "nearbyEmptyNotice"
        Layout.fillWidth: true
        visible: root.discoveryController.scanning
                 && root.discoveryController.services.length === 0
                 && root.discoveryController.unavailableReason.length === 0
        text: qsTr("No servers have announced themselves yet.")
        color: root.palette.placeholderText
        wrapMode: Text.WordWrap
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Repeater {
        model: root.discoveryController.services

        NearbyServerRow {
            required property var modelData
            Layout.fillWidth: true
            service: modelData
            onOpenRequested: root.openRequested(modelData.key)
            onSaveRequested: root.saveRequested(modelData.key)
        }
    }
}

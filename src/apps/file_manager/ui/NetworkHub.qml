// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Network place's destination (ADR-0194): the saved locations, the way
// to add one, and a plain statement of how signing in works. Discovery of
// nearby servers is a later slice and is deliberately not implied here --
// the hub shows only what the user saved.
Control {
    id: root
    objectName: "networkHub"

    required property var networkLocationsController
    required property var discoveryController

    // The chosen location's canonical address, for the window to navigate to.
    signal openRequested(string url)
    signal connectRequested()
    // A nearby server the user asked to keep, as a canonical address the
    // Connect-to-server dialog opens pre-filled.
    signal saveRequested(string url)

    padding: 16
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Network locations")

    contentItem: ColumnLayout {
        spacing: 12


        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: qsTr("Network")
                font.bold: true
                font.pointSize: root.font.pointSize + 3
                Accessible.ignored: true
            }
            Button {
                objectName: "connectToServerButton"
                text: qsTr("Connect to Server…")
                Accessible.description: qsTr("Save and open a network folder")
                onClicked: root.connectRequested()
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Saved locations open like any folder. Sign-in is handled by the "
                     + "system when a server asks for it; QindaQt stores no password. "
                     + "SFTP uses your SSH keys and ~/.ssh/config.")
            color: root.palette.placeholderText
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }

        StatusBanner {
            objectName: "networkLocationsErrorBanner"
            Layout.fillWidth: true
            visible: root.networkLocationsController.storeError.length > 0
            title: qsTr("Saved locations could not be read")
            message: root.networkLocationsController.storeError
            actionText: qsTr("Dismiss")
            onActionTriggered: root.networkLocationsController.clearStoreError()
        }

        Label {
            objectName: "networkHubEmptyNotice"
            Layout.fillWidth: true
            visible: root.networkLocationsController.locations.length === 0
            text: qsTr("No locations saved yet. Choose “Connect to Server…” to add one, "
                     + "for example sftp://qinda/mnt/storage.")
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        NearbyServersSection {
            Layout.fillWidth: true
            discoveryController: root.discoveryController
            onOpenRequested: (url) => root.openRequested(url)
            onSaveRequested: (url) => root.saveRequested(url)
        }

        Label {
            Layout.topMargin: 6
            visible: root.networkLocationsController.locations.length > 0
            text: qsTr("Saved")
            font.bold: true
            Accessible.ignored: true
        }

        ListView {
            id: locationList
            objectName: "networkLocationList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            boundsBehavior: Flickable.StopAtBounds
            model: root.networkLocationsController.locations
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Saved network locations")

            delegate: NetworkLocationCard {
                required property var modelData
                width: locationList.width
                location: modelData
                onOpenRequested: root.openRequested(modelData.url)
                onRemoveRequested: root.networkLocationsController.removeLocation(modelData.index)
            }
        }
    }
}

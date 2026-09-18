// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Discovery, the Connect dialog's default, and what mounting at login costs
// (ADR-0197/0199). The mount list is read from the manager rather than from
// the locations, so it shows what is actually on disk.
ColumnLayout {
    id: root
    objectName: "preferencesNetworkPage"

    required property var preferencesController
    required property var discoveryController
    required property var networkLocationsController
    required property var mountManager

    spacing: 10

    CheckBox {
        objectName: "preferenceDiscoverBox"
        text: qsTr("Look for servers on this network")
        enabled: root.discoveryController.supported
        checked: root.preferencesController.discoverNearbyServers
        Accessible.name: text
        onToggled: root.preferencesController.setDiscoverNearbyServers(checked)
    }

    Label {
        Layout.fillWidth: true
        text: root.discoveryController.supported
            ? qsTr("Asks the system's Avahi service which machines advertise SFTP or "
                 + "Windows sharing. Nothing is connected to until you open it.")
            : qsTr("This machine has no service-discovery provider, so nearby servers "
                 + "cannot be listed. Saved locations are unaffected.")
        color: root.palette.placeholderText
        wrapMode: Text.WordWrap
        Accessible.ignored: true
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        rowSpacing: 8

        Label { text: qsTr("Connect to server uses"); Accessible.ignored: true }
        ComboBox {
            objectName: "preferenceDefaultSchemeBox"
            Layout.fillWidth: true
            model: root.preferencesController.schemes
            currentIndex: model.indexOf(root.preferencesController.defaultConnectScheme)
            Accessible.name: qsTr("Default connection type")
            onActivated: root.preferencesController.setDefaultConnectScheme(currentText)
        }
    }

    Label {
        Layout.topMargin: 6
        text: qsTr("Mounted at login")
        font.bold: true
        Accessible.ignored: true
    }

    Label {
        objectName: "preferenceMountSummary"
        Layout.fillWidth: true
        text: root.mountManager.mountedLocationCount === 0
            ? qsTr("No location is set to mount at login. Turn it on for an SFTP "
                 + "location in Connect to Server, and it appears under ~/Network so "
                 + "applications that do not speak SFTP can reach it. That needs "
                 + "sshfs installed; without it the mount fails and says so in the "
                 + "system journal.")
            : qsTr("%1 location(s) mount under ~/Network at login.")
                  .arg(root.mountManager.mountedLocationCount)
        color: root.palette.placeholderText
        wrapMode: Text.WordWrap
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    StatusBanner {
        objectName: "preferenceMountErrorBanner"
        Layout.fillWidth: true
        visible: root.mountManager.lastError.length > 0
        title: qsTr("Mount at login")
        message: root.mountManager.lastError
        actionText: qsTr("Dismiss")
        onActionTriggered: root.mountManager.clearLastError()
    }

    StatusBanner {
        objectName: "preferenceLocationsErrorBanner"
        Layout.fillWidth: true
        visible: root.networkLocationsController.storeError.length > 0
        title: qsTr("Saved locations")
        message: root.networkLocationsController.storeError
        actionText: qsTr("Dismiss")
        onActionTriggered: root.networkLocationsController.clearStoreError()
    }

    Item { Layout.fillHeight: true }
}

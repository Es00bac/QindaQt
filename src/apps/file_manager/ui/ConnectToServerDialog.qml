// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Collects one network address and saves it (ADR-0194). Every refusal comes
// back from C++ (buildNetworkLocation) and is shown verbatim, so the dialog
// itself validates nothing and cannot disagree with the store.
//
// There is deliberately no user-name or password field: a saved location
// never carries userinfo, and QindaQt writes no secret (ADR-0196). When a
// server asks for credentials the platform's own KIO prompt handles it.
Dialog {
    id: root
    objectName: "connectToServerDialog"

    required property var networkLocationsController
    required property var preferencesController

    // Emitted with the saved location's canonical address after a successful
    // save, so the window can navigate straight into it.
    signal saved(string url)

    anchors.centerIn: parent
    width: Math.min(560, (parent ? parent.width : 560) - 32)
    modal: true
    title: qsTr("Connect to server")
    standardButtons: Dialog.Ok | Dialog.Cancel

    // Opens empty, or pre-filled from a canonical address (a nearby server
    // the user chose to keep). The address is parsed in QML only to fill the
    // fields; buildNetworkLocation() still decides what is savable.
    function prepare(address) {
        root.networkLocationsController.clearRequestError()
        const schemes = root.networkLocationsController.schemes
        const preferred = root.preferencesController.defaultConnectScheme
        schemeBox.currentIndex = Math.max(0, schemes.indexOf(preferred))
        hostField.text = ""
        portField.text = ""
        remotePathField.text = ""
        displayNameField.text = ""
        showInPlacesBox.checked = true
        mountAtLoginBox.checked = false
        if (!address)
            return
        const parsed = /^([a-z]+):\/\/([^/:]+)(?::(\d+))?(\/.*)?$/.exec(address)
        if (!parsed)
            return
        const schemeIndex = schemes.indexOf(parsed[1])
        if (schemeIndex >= 0)
            schemeBox.currentIndex = schemeIndex
        hostField.text = parsed[2]
        portField.text = parsed[3] ? parsed[3] : ""
        remotePathField.text = parsed[4] ? parsed[4] : ""
    }

    onAccepted: {
        const url = root.networkLocationsController.saveLocation({
            "scheme": schemeBox.currentText,
            "host": hostField.text,
            "port": portField.text,
            "remotePath": remotePathField.text,
            "displayName": displayNameField.text,
            "showInPlaces": showInPlacesBox.checked,
            "mountAtLogin": mountAtLoginBox.checked
        })
        if (url.length > 0) {
            root.saved(url)
            return
        }
        // A refusal must not lose what the user typed. accept() has already
        // begun closing, so the dialog is re-opened after that completes,
        // with every field and the refusal banner still in place.
        Qt.callLater(function() { root.open() })
    }

    contentItem: ColumnLayout {
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 6

            Label { text: qsTr("Type"); Accessible.ignored: true }
            ComboBox {
                id: schemeBox
                objectName: "connectSchemeBox"
                Layout.fillWidth: true
                model: root.networkLocationsController.schemes
                Accessible.name: qsTr("Connection type")
            }

            Label { text: qsTr("Server"); Accessible.ignored: true }
            TextField {
                id: hostField
                objectName: "connectHostField"
                Layout.fillWidth: true
                placeholderText: qsTr("name or address, for example qinda")
                Accessible.name: qsTr("Server name or address")
            }

            Label { text: qsTr("Port"); Accessible.ignored: true }
            TextField {
                id: portField
                objectName: "connectPortField"
                Layout.fillWidth: true
                inputMethodHints: Qt.ImhDigitsOnly
                placeholderText: qsTr("leave empty for the default")
                Accessible.name: qsTr("Port")
            }

            Label { text: qsTr("Folder"); Accessible.ignored: true }
            TextField {
                id: remotePathField
                objectName: "connectPathField"
                Layout.fillWidth: true
                placeholderText: qsTr("/mnt/storage")
                Accessible.name: qsTr("Folder on the server")
            }

            Label { text: qsTr("Name"); Accessible.ignored: true }
            TextField {
                id: displayNameField
                objectName: "connectNameField"
                Layout.fillWidth: true
                placeholderText: qsTr("leave empty to name it after the folder")
                Accessible.name: qsTr("Name for this location")
            }
        }

        CheckBox {
            id: showInPlacesBox
            objectName: "connectShowInPlacesBox"
            text: qsTr("Show in the sidebar")
            checked: true
            Accessible.name: text
        }

        CheckBox {
            id: mountAtLoginBox
            objectName: "connectMountAtLoginBox"
            // ADR-0199: sshfs is the only mount this knob knows, so it is
            // unavailable for Windows sharing rather than silently ignored.
            enabled: schemeBox.currentText === "sftp"
            text: qsTr("Mount under ~/Network at login")
            Accessible.name: text
            Accessible.description: qsTr("Makes this location reachable by "
                + "applications that do not speak SFTP. Requires sshfs.")
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("A password, if the server needs one, is asked for by the system "
                     + "when connecting. QindaQt never stores it.")
            color: root.palette.placeholderText
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }

        StatusBanner {
            objectName: "connectRequestErrorBanner"
            Layout.fillWidth: true
            visible: root.networkLocationsController.requestError.length > 0
            title: qsTr("That location cannot be saved")
            message: root.networkLocationsController.requestError
            actionText: qsTr("Dismiss")
            onActionTriggered: root.networkLocationsController.clearRequestError()
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The File Manager's preferences (ADR-0198), opened with Ctrl+, or from the
// File menu. A separate window rather than a modal dialog, because changing a
// default is something a user does while looking at the folder it affects.
//
// Presentation only: every control writes through PreferencesController,
// which validates, persists, and republishes. Nothing here holds a value.
ApplicationWindow {
    id: root
    objectName: "preferencesWindow"

    required property var preferencesController
    required property var discoveryController
    required property var networkLocationsController
    required property var mountManager

    title: qsTr("File Manager Preferences")
    width: 560
    height: 440
    minimumWidth: 420
    minimumHeight: 320
    modality: Qt.NonModal
    flags: Qt.Dialog

    // A preferences window is closed, not quit: the application's own quit
    // arbitration belongs to the main window.
    onClosing: function(close) { close.accepted = true }

    header: TabBar {
        id: tabs
        objectName: "preferencesTabBar"
        TabButton { objectName: "preferencesTab_general"; text: qsTr("General") }
        TabButton { objectName: "preferencesTab_views"; text: qsTr("Views") }
        TabButton { objectName: "preferencesTab_network"; text: qsTr("Network") }
        TabButton { objectName: "preferencesTab_trash"; text: qsTr("Trash") }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        StatusBanner {
            objectName: "preferencesStoreBanner"
            Layout.fillWidth: true
            visible: root.preferencesController.storeError.length > 0
            title: qsTr("Preferences could not be saved")
            message: root.preferencesController.storeError
            actionText: qsTr("Dismiss")
            onActionTriggered: root.preferencesController.clearStoreError()
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            ScrollView {
                id: generalScroller
                contentWidth: availableWidth
                clip: true
                PreferencesGeneralPage {
                    width: generalScroller.availableWidth
                    preferencesController: root.preferencesController
                }
            }
            ScrollView {
                id: viewsScroller
                contentWidth: availableWidth
                clip: true
                PreferencesViewsPage {
                    width: viewsScroller.availableWidth
                    preferencesController: root.preferencesController
                }
            }
            ScrollView {
                id: networkScroller
                contentWidth: availableWidth
                clip: true
                PreferencesNetworkPage {
                    width: networkScroller.availableWidth
                    preferencesController: root.preferencesController
                    discoveryController: root.discoveryController
                    networkLocationsController: root.networkLocationsController
                    mountManager: root.mountManager
                }
            }
            ScrollView {
                id: trashScroller
                contentWidth: availableWidth
                clip: true
                PreferencesTrashPage {
                    width: trashScroller.availableWidth
                    preferencesController: root.preferencesController
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                objectName: "preferencesRestoreDefaultsButton"
                text: qsTr("Restore Defaults")
                Accessible.description: qsTr("Return every preference to its default")
                onClicked: root.preferencesController.restoreDefaults()
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "preferencesCloseButton"
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}

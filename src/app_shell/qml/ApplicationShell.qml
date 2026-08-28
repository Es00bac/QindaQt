// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

ApplicationWindow {
    id: root

    required property ApplicationCoordinator coordinator
    required property Item initialFocusItem
    property bool closeAuthorized: false
    default property alias pageContent: pageHost.data

    visible: true
    width: 920
    height: 680
    minimumWidth: 420
    minimumHeight: 320
    title: coordinator.windowTitle.length > 0
           ? coordinator.windowTitle : coordinator.applicationName
    color: Tokens.bg.base

    // AGENT-CONTRACT: Closing asks the owning application for a decision. The
    // coordinator and this surface never call QCoreApplication::quit or infer
    // whether domain state is safe to discard.
    onClosing: function(close) {
        if (closeAuthorized) {
            close.accepted = true
            return
        }
        close.accepted = false
        coordinator.requestQuit("window-close")
    }

    onActiveFocusItemChanged: {
        const owner = activeFocusItem && activeFocusItem.objectName
                    ? activeFocusItem.objectName : ""
        coordinator.reportFocusOwner(owner)
    }

    Component.onCompleted: {
        if (initialFocusItem
                && (coordinator.initialFocusObjectName.length === 0
                    || coordinator.initialFocusObjectName === initialFocusItem.objectName))
            initialFocusItem.forceActiveFocus(Qt.TabFocusReason)
    }

    Connections {
        target: coordinator
        function onQuitApproved(requestId) {
            root.closeAuthorized = true
            root.close()
        }
    }

    menuBar: MenuBar {
        id: exportedMenuBar
        objectName: "appShellMenuBar"

        Instantiator {
            id: menuFactory
            model: coordinator.menus

            delegate: Menu {
                id: exportedMenu
                required property var modelData
                title: modelData.label

                Instantiator {
                    model: exportedMenu.modelData.actions

                    delegate: Action {
                        required property var modelData
                        text: modelData.label
                        enabled: modelData.enabled
                        checkable: modelData.checkable
                        checked: modelData.checked
                        shortcut: modelData.shortcut
                        property string accessibleDescription: modelData.accessibleDescription
                        onTriggered: coordinator.activateAction(modelData.id)
                    }

                    onObjectAdded: function(index, object) {
                        exportedMenu.insertAction(index, object)
                    }
                    onObjectRemoved: function(index, object) {
                        exportedMenu.removeAction(object)
                    }
                }
            }

            onObjectAdded: function(index, object) {
                exportedMenuBar.insertMenu(index, object)
            }
            onObjectRemoved: function(index, object) {
                exportedMenuBar.removeMenu(object)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Tokens.space["3"]

        Qinda.DegradedNotice {
            id: degradedNotice
            objectName: "appShellDegradedNotice"
            Layout.fillWidth: true
            Layout.margins: Tokens.space["4"]
            visible: coordinator.degraded
            // AGENT-CONTRACT: A degraded integration remains usable. Keep its
            // visible and accessible title distinct from an unavailable one.
            title: coordinator.hasUnavailableIntegration
                   ? qsTr("Feature unavailable") : qsTr("Limited capability")
            reason: coordinator.degradedMessage
            retryText: ""
        }

        Item {
            id: pageHost
            objectName: "appShellPageHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Accessible.role: Accessible.Pane
            Accessible.name: coordinator.applicationName.length > 0
                             ? coordinator.applicationName : root.title
            Accessible.description: coordinator.degraded
                                    ? coordinator.degradedMessage : ""
        }
    }
}

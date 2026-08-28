// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

ApplicationWindow {
    id: root

    required property var navigationController

    visible: true
    width: 900
    height: 600
    minimumWidth: 480
    minimumHeight: 320
    title: qsTr("QindaQt File Manager — %1").arg(navigationController.currentPath)
    color: Tokens.bg.base

    Shortcut { sequence: "Alt+Left"; onActivated: root.navigationController.goBack() }
    Shortcut { sequence: "Alt+Right"; onActivated: root.navigationController.goForward() }
    Shortcut { sequence: "Alt+Up"; onActivated: root.navigationController.goUp() }
    Shortcut { sequence: "F5"; onActivated: root.navigationController.refresh() }
    Shortcut { sequence: "Ctrl+R"; onActivated: root.navigationController.refresh() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Toolbar {
            Layout.fillWidth: true
            navigationController: root.navigationController
        }

        Breadcrumb {
            Layout.fillWidth: true
            navigationController: root.navigationController
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.navigationController.statusKey === "ready" ? 0 : 1

            EntryList {
                navigationController: root.navigationController
            }

            StatePane {
                statusKey: root.navigationController.statusKey
                statusMessage: root.navigationController.statusMessage
                onRetryRequested: root.navigationController.refresh()
            }
        }

        Qinda.StateCard {
            objectName: "launchErrorBanner"
            Layout.fillWidth: true
            visible: root.navigationController.launchError.length > 0
            status: Qinda.StateCard.Warning
            title: qsTr("Couldn't open the file")
            message: root.navigationController.launchError
            actionText: qsTr("Dismiss")
            onActionTriggered: root.navigationController.clearLaunchError()
        }
    }
}

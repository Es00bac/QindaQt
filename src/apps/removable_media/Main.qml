// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as QQ
import QindaQt.Tokens 1.0

T.ApplicationWindow {
    id: root
    objectName: "removableMediaWindow"
    width: 760
    height: 720
    minimumWidth: 560
    minimumHeight: 540
    visible: !mediaWatchMode
    title: qsTr("Removable Media")
    color: Tokens.bg.base

    // AGENT-CONTRACT: closing the window hides the session's one watcher.
    // Explicit Activate() and notification actions reopen this same window.
    onClosing: function(close) { close.accepted = false; root.hide() }

    Connections {
        target: mediaController
        function onWindowRequested(token) {
            root.show()
            root.raise()
            root.requestActivate()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["6"]
        spacing: Tokens.space["4"]

        RowLayout {
            Layout.fillWidth: true
            QQ.SectionHeader {
                Layout.fillWidth: true
                title: qsTr("Removable media")
                description: qsTr("Choose what to do with your USB drives, discs, and external storage.")
            }
            QQ.Button {
                objectName: "refreshMediaButton"
                text: qsTr("Refresh")
                emphasized: false
                busy: mediaController.busy
                onClicked: mediaController.refresh()
            }
        }

        QQ.DegradedNotice {
            Layout.fillWidth: true
            visible: !mediaController.available
            title: qsTr("Storage service unavailable")
            reason: mediaController.status || qsTr("The storage service could not be reached. Try again in a moment.")
            retryText: qsTr("Retry")
            onRetryRequested: mediaController.refresh()
        }
        QQ.StateCard {
            objectName: "mediaStatus"
            Layout.fillWidth: true
            visible: mediaController.available && mediaController.status.length > 0
            status: mediaController.busy ? QQ.StateCard.Busy : QQ.StateCard.Information
            title: mediaController.busy ? qsTr("Working…") : qsTr("Storage status")
            message: mediaController.status
        }
        QQ.StateCard {
            Layout.fillWidth: true
            visible: mediaLaunchError.length > 0
            status: QQ.StateCard.Error
            title: qsTr("Could not open File Manager")
            message: mediaLaunchError
        }

        QQ.FormSurface {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(190, Math.max(84, mediaList.contentHeight + Tokens.space["4"]))
            ListView {
                id: mediaList
                objectName: "mediaList"
                anchors.fill: parent
                anchors.margins: Tokens.space["2"]
                clip: true
                spacing: Tokens.space["2"]
                model: mediaController.volumes
                T.ScrollBar.vertical: T.ScrollBar {}
                delegate: QQ.Button {
                    required property var modelData
                    width: mediaList.width
                    emphasized: modelData.token === (mediaController.selected.token || "")
                    text: (modelData.label || modelData.device) + " · " + modelData.kind
                          + " · " + modelData.sizeText
                          + (modelData.mounted ? qsTr(" · Mounted") : "")
                    accessibleDescription: modelData.device
                    onClicked: mediaController.select(modelData.token)
                }
                QQ.Label {
                    anchors.centerIn: parent
                    visible: mediaList.count === 0
                    text: qsTr("Insert a USB drive or disc to get started.")
                    muted: true
                }
            }
        }

        T.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            MediaDetails {
                width: parent.width
                controller: mediaController
                volume: mediaController.selected
            }
        }
    }

    FormatDialog {
        id: formatDialog
        controller: mediaController
        parent: T.Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        width: Math.min(520, root.width - Tokens.space["8"])
    }
}

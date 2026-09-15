// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Applications browser (ADR-0164): a Finder-style drill-down over the
// installed-application tree. Stock Qt Quick Controls only (ADR-0116); the
// view holds no state of its own — every folder change and launch crosses
// the injected ApplicationsController, exactly like the browsing views cross
// the NavigationController.
ColumnLayout {
    id: root
    required property var applicationsController
    // ADR-0165: in chooser mode every entry activation becomes a workspace
    // choice, so terminal/D-Bus entries are enabled too (the compositor
    // launches them) and the view explains its purpose.
    property bool chooserMode: false

    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        spacing: 4

        Button {
            objectName: "applicationsBackButton"
            flat: true
            icon.name: "go-previous"
            enabled: root.applicationsController.breadcrumb.length > 0
            Accessible.name: qsTr("Back to the previous folder")
            onClicked: root.applicationsController.openParentFolder()
        }

        Label {
            objectName: "applicationsBreadcrumb"
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            text: root.applicationsController.breadcrumb.length > 0
                  ? qsTr("Applications") + " › " + root.applicationsController.breadcrumb
                  : qsTr("Applications")
            font.bold: true
        }
    }

    Label {
        objectName: "chooserHint"
        Layout.fillWidth: true
        visible: root.chooserMode
        text: qsTr("Pick the application that will replace this picker in the layout.")
        wrapMode: Text.WordWrap
        font.italic: true
    }

    ListView {
        id: folderColumn
        Layout.fillWidth: true
        // Fixed height: any binding back to the ColumnLayout's own size
        // (root.height/contentHeight) rearranges recursively inside Layouts.
        Layout.preferredHeight: 132
        clip: true
        spacing: 2
        model: root.applicationsController.folders
        delegate: ItemDelegate {
            required property var modelData
            width: folderColumn.width
            text: modelData.label
            icon.name: "folder"
            Accessible.name: modelData.label
            onClicked: root.applicationsController.openFolder(
                           modelData.id === "root" ? "" : modelData.id)
        }
        Label {
            anchors.centerIn: parent
            visible: folderColumn.count === 0
            text: qsTr("No folders")
            color: palette.placeholderText
        }
    }

    ListView {
        id: entryColumn
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 2
        model: root.applicationsController.entries
        delegate: ItemDelegate {
            id: entryDelegate
            required property var modelData
            width: entryColumn.width
            enabled: root.chooserMode || modelData.launchable
            onClicked: root.chooserMode
                ? root.applicationsController.chooseForWorkspace(modelData.id)
                : root.applicationsController.activateEntry(modelData.id)

            contentItem: RowLayout {
                spacing: 8
                Image {
                    sourceSize: Qt.size(24, 24)
                    source: "image://theme-icons/"
                            + (entryDelegate.modelData.iconName
                               || "application-octet-stream")
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: entryDelegate.modelData.name
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: entryDelegate.modelData.message.length > 0
                                 && !root.chooserMode
                        text: entryDelegate.modelData.message
                        elide: Text.ElideRight
                        color: palette.placeholderText
                        font.pointSize: Qt.application.font.pointSize - 1
                    }
                }
            }
            Accessible.name: entryDelegate.modelData.name
            Accessible.description: entryDelegate.modelData.comment
        }
        Label {
            anchors.centerIn: parent
            visible: entryColumn.count === 0
            text: qsTr("No applications here")
            color: palette.placeholderText
        }
    }
}

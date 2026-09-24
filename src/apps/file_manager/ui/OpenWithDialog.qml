// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Open With's chooser (ADR-0269): the applications that handle the files
// first, then every application the Applications place lists, and "Always
// open ... with this application" for the files' shared type. Stock Controls
// only (ADR-0116); every effect goes through OpenWithController, which also
// reports failures (the window's banner shows them).
Dialog {
    id: root
    objectName: "openWithDialog"

    property var controller: null
    property var paths: []
    property string mimeType: ""
    property var recommended: []
    property var others: []
    readonly property var choices: root.recommended.concat(root.others)
    readonly property var chosen: applicationList.currentIndex >= 0
        && applicationList.currentIndex < root.choices.length
        ? root.choices[applicationList.currentIndex] : null

    function openFor(filePaths) {
        if (!root.controller || filePaths.length === 0)
            return
        root.paths = filePaths
        root.mimeType = root.controller.mimeTypeFor(filePaths)
        root.recommended = root.controller.candidatesFor(filePaths)
        const recommendedIds = root.recommended.map(application => application.id)
        root.others = root.controller.allApplications()
            .filter(application => recommendedIds.indexOf(application.id) < 0)
        alwaysBox.checked = false
        applicationList.currentIndex = root.choices.length > 0 ? 0 : -1
        open()
        applicationList.forceActiveFocus()
    }

    function fileName(path) {
        return path.substring(path.lastIndexOf("/") + 1)
    }

    title: root.paths.length === 1
        ? qsTr("Open “%1” With").arg(root.fileName(root.paths[0]))
        : qsTr("Open %1 Files With").arg(root.paths.length)
    modal: true
    anchors.centerIn: parent
    width: Math.min(480, parent ? parent.width - 48 : 480)
    height: Math.min(520, parent ? parent.height - 48 : 520)
    standardButtons: Dialog.Open | Dialog.Cancel

    onAccepted: {
        if (!root.chosen || !root.controller)
            return
        // The default is recorded first, so a file opened by it and the next
        // double-click agree; a failed write is reported, the file still opens.
        if (alwaysBox.checked && root.mimeType.length > 0)
            root.controller.setDefault(root.mimeType, root.chosen.id)
        root.controller.openWith(root.chosen.id, root.paths)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        ListView {
            id: applicationList
            objectName: "openWithApplicationList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.choices
            keyNavigationEnabled: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Applications")
            Keys.onReturnPressed: root.accept()
            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width

                // Section headings: the handlers of this file type, then all.
                Label {
                    visible: (row.index === 0 && root.recommended.length > 0)
                        || (row.index === root.recommended.length && root.others.length > 0)
                    text: row.index === 0 && root.recommended.length > 0
                        ? qsTr("Recommended") : qsTr("All Applications")
                    font.bold: true
                    topPadding: 6
                    leftPadding: 6
                }
                ItemDelegate {
                    width: parent.width
                    text: row.modelData.isDefault
                        ? qsTr("%1 (default)").arg(row.modelData.name) : row.modelData.name
                    icon.name: row.modelData.iconName
                    highlighted: applicationList.currentIndex === row.index
                    onClicked: applicationList.currentIndex = row.index
                    onDoubleClicked: {
                        applicationList.currentIndex = row.index
                        root.accept()
                    }
                }
            }
        }

        CheckBox {
            id: alwaysBox
            objectName: "openWithAlwaysBox"
            Layout.fillWidth: true
            // A mixed selection has no single type to make a default for.
            enabled: root.mimeType.length > 0 && root.chosen !== null
            text: qsTr("Always open %1 files with this application")
                .arg(root.controller && root.mimeType.length > 0
                     ? (root.controller.mimeDescription(root.mimeType) || root.mimeType)
                     : qsTr("these"))
        }
    }
}

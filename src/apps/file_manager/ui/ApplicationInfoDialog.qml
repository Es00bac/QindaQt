// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Get Info for one application of the Applications place (ADR-0262), in the
// PropertiesDialog pattern: stock Controls only (ADR-0116), every value read
// from ApplicationsController.describe(), no policy of its own. A missing
// value is left out rather than shown as a blank row.
Dialog {
    id: root

    // The describe() map of the application being shown; empty when closed.
    property var info: ({})

    function show(description) {
        root.info = description || ({})
        if (root.info.id)
            open()
    }

    objectName: "applicationInfoDialog"
    title: qsTr("Get Info — %1").arg(root.info.name || "")
    modal: true
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    width: Math.min(460, parent ? parent.width - 48 : 460)
    onClosed: root.info = ({})

    component InfoRow: RowLayout {
        id: infoRow
        required property string label
        required property string value
        required property string key
        Layout.fillWidth: true
        visible: infoRow.value.length > 0
        spacing: 12
        Label {
            text: infoRow.label
            font.bold: true
            Layout.preferredWidth: 120
            Layout.alignment: Qt.AlignTop
            Accessible.ignored: true
        }
        Label {
            objectName: "applicationInfo_" + infoRow.key
            text: infoRow.value
            Layout.fillWidth: true
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            Accessible.name: infoRow.label + " " + infoRow.value
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            spacing: 12
            Image {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                sourceSize: Qt.size(48, 48)
                source: root.info.iconName ? "image://theme-icons/" + root.info.iconName : ""
                Accessible.ignored: true
            }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Label {
                    text: root.info.name || ""
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Label {
                    visible: text.length > 0
                    text: root.info.genericName || ""
                    color: root.palette.placeholderText
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
            }
        }

        InfoRow { key: "comment"; label: qsTr("Description:"); value: root.info.comment || "" }
        InfoRow { key: "category"; label: qsTr("Category:"); value: root.info.category || "" }
        InfoRow { key: "categories"; label: qsTr("Categories:"); value: root.info.categories || "" }
        InfoRow { key: "command"; label: qsTr("Command:"); value: root.info.command || "" }
        InfoRow { key: "note"; label: qsTr("Starts:"); value: root.info.note || "" }
        InfoRow { key: "desktopFilePath"; label: qsTr("Desktop entry:"); value: root.info.desktopFilePath || "" }
    }
}

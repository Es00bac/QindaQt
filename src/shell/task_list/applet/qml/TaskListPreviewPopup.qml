// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Hover thumbnail card (ADR-0119). One popup window per dock strip: it is
// never clipped by the panel band and can paint above the hovered tile.
// A zero image token means the compositor could not provide a capture; the
// card then degrades to the title-only form (never a second tooltip).
T.Popup {
    id: popup

    property string titleText: ""
    property int imageToken: 0
    readonly property bool hasImage: imageToken > 0
        && previewImage.status === Image.Ready

    objectName: "taskListPreviewPopup"
    popupType: T.Popup.Window
    closePolicy: T.Popup.NoAutoClose
    padding: Tokens.space["2"]
    width: previewImage.width + padding * 2
    height: previewColumn.implicitHeight + padding * 2

    background: Rectangle {
        radius: Tokens.ready ? Tokens.radius.l : 0
        color: Tokens.ready ? Tokens.bg.raised : "transparent"
        border.color: Tokens.ready ? Tokens.outline.divider : "transparent"
        border.width: 1
    }

    contentItem: ColumnLayout {
        id: previewColumn
        spacing: Tokens.space["2"]

        Image {
            id: previewImage
            objectName: "taskListPreviewImage"
            visible: imageToken > 0
            Layout.maximumWidth: 320
            Layout.maximumHeight: 200
            Layout.alignment: Qt.AlignHCenter
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: false
            source: imageToken > 0
                ? "image://qindaqt-task-preview/" + imageToken : ""
            Accessible.ignored: true
        }

        RowLayout {
            spacing: Tokens.space["2"]

            C.Label {
                id: titleLabel
                objectName: "taskListPreviewTitle"
                Layout.maximumWidth: 300
                text: popup.titleText
                elide: Text.ElideRight
                Accessible.role: Accessible.StaticText
                Accessible.name: popup.titleText
            }
        }
    }
}

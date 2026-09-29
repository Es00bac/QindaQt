// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaTK as Tk

// The finished capture and what to do with it. Save never overwrites; the
// notification a save posts carries the same Open / Copy / Show in folder.
Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        spacing: Tk.Theme.space.md

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            color: Tk.Theme.color.canvas
            border.color: Tk.Theme.color.border
            border.width: Tk.Theme.size.border
            radius: Tk.Theme.radius.sm

            Image {
                objectName: "resultImage"
                anchors.fill: parent
                anchors.margins: Tk.Theme.space.sm
                source: captureResult.source
                cache: false
                asynchronous: false
                smooth: true
                mipmap: true
                fillMode: Image.PreserveAspectFit
                Accessible.role: Accessible.Graphic
                Accessible.name: qsTr("Screenshot, %1 by %2 pixels")
                    .arg(captureResult.size.width).arg(captureResult.size.height)
            }
        }

        Tk.Caption {
            objectName: "resultSummary"
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            text: captureResult.savedPath.length > 0
                  ? qsTr("%1 × %2 pixels · saved to %3").arg(captureResult.size.width)
                        .arg(captureResult.size.height).arg(captureResult.savedPath)
                  : qsTr("%1 × %2 pixels · not saved yet").arg(captureResult.size.width)
                        .arg(captureResult.size.height)
        }

        Tk.Notice {
            objectName: "resultNotice"
            Layout.fillWidth: true
            visible: captureResult.message.length > 0
            variant: captureResult.messageIsError ? "danger" : "success"
            text: captureResult.message
        }

        Flow {
            Layout.fillWidth: true
            spacing: Tk.Theme.space.sm
            Tk.Button {
                objectName: "copyButton"
                iconName: "copy"
                text: qsTr("Copy")
                tooltip: qsTr("Copy the image to the clipboard (Ctrl+C)")
                onClicked: captureResult.copy()
            }
            Tk.Button {
                objectName: "saveButton"
                variant: "accent"
                iconName: "save"
                text: qsTr("Save")
                tooltip: qsTr("Save to the screenshot folder without replacing any file (Ctrl+S)")
                onClicked: captureResult.save()
            }
            Tk.Button {
                objectName: "saveAsButton"
                iconName: "file-image"
                text: qsTr("Save As…")
                tooltip: qsTr("Choose where to save (Ctrl+Shift+S)")
                onClicked: page.saveAsRequested()
            }
            Tk.Button {
                objectName: "openButton"
                iconName: "external-link"
                text: qsTr("Open")
                tooltip: qsTr("Open in the image viewer, saving first if needed")
                onClicked: captureResult.open()
            }
            Tk.Button {
                objectName: "showInFolderButton"
                iconName: "folder-open"
                text: qsTr("Show in folder")
                tooltip: qsTr("Show the saved file in the file manager, saving first if needed")
                onClicked: captureResult.showInFolder()
            }
            Tk.Button {
                objectName: "newCaptureButton"
                iconName: "camera"
                text: qsTr("New screenshot")
                tooltip: qsTr("Take another screenshot with the same options (Ctrl+N)")
                enabled: captureFlow.phase === "idle"
                onClicked: captureFlow.start()
            }
            Tk.Button {
                objectName: "discardButton"
                iconName: "x"
                text: qsTr("Discard")
                tooltip: qsTr("Go back to the capture options")
                onClicked: captureResult.clear()
            }
        }
    }

    signal saveAsRequested()
}

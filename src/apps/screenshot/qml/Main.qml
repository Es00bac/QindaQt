// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QindaTK as Tk
import QindaTK.QindaQt

// The Screenshot window: capture options or the finished capture, and the
// OBS record control.
//
// AGENT-CONTRACT: `captureFlow`, `captureResult`, `recorder`,
// `screenshotPreferences` and `screenshotApp` are the C++ context objects
// (ADR-0289). Every enablement here mirrors one of their predicates; this
// file lays out and never decides on its own that an action is possible.
// The window starts hidden: ScreenshotApp shows it when a flow needs it, and
// the region overlays below are separate top-level windows that work while
// this one is hidden.
Tk.AppWindow {
    id: window
    objectName: "screenshotWindow"

    visible: false
    width: 640
    height: 540
    minimumWidth: 460
    minimumHeight: 420
    title: qsTr("Screenshot")

    // Feeds the desktop's semantic tokens (light/dark, Corner Bar themes and
    // accessibility settings) into Tk.Theme.
    QindaQtTheme {}

    property int currentTab: 0

    QtObject {
        id: regionState
        property rect selection: Qt.rect(0, 0, 0, 0)
        property point anchor: Qt.point(0, 0)
        property bool dragging: false
        property string hint: ""
    }

    Connections {
        target: captureFlow
        function onPhaseChanged() {
            if (captureFlow.phase === "selecting") {
                regionState.selection = Qt.rect(0, 0, 0, 0)
                regionState.dragging = false
                regionState.hint = ""
            }
        }
        function onSelectionRejected(message) { regionState.hint = message }
    }

    Shortcut {
        sequences: [StandardKey.Copy]
        enabled: window.currentTab === 0 && captureResult.hasImage
        onActivated: captureResult.copy()
    }
    Shortcut {
        sequences: [StandardKey.Save]
        enabled: window.currentTab === 0 && captureResult.hasImage
        onActivated: captureResult.save()
    }
    Shortcut {
        sequences: ["Ctrl+Shift+S"]
        enabled: window.currentTab === 0 && captureResult.hasImage
        onActivated: window.openSaveAs()
    }
    Shortcut {
        sequences: [StandardKey.New]
        enabled: captureFlow.phase === "idle"
        onActivated: { window.currentTab = 0; captureFlow.start() }
    }
    Shortcut {
        sequences: [StandardKey.Close, StandardKey.Quit]
        onActivated: window.close()
    }

    function openSaveAs() {
        saveDialog.currentFolder = captureResult.saveFolderUrl()
        saveDialog.selectedFile = captureResult.suggestedSaveUrl()
        saveDialog.open()
    }

    toolBars: Tk.ToolBar {
        objectName: "screenshotToolbar"
        Tk.TabStrip {
            objectName: "screenshotTabs"
            model: [
                { "text": qsTr("Screenshot"), "iconName": "camera" },
                { "text": qsTr("Record"), "iconName": "video" }
            ]
            currentIndex: window.currentTab
            onTabActivated: index => window.currentTab = index
        }
    }

    Item {
        CapturePage {
            objectName: "capturePage"
            anchors.fill: parent
            anchors.margins: Tk.Theme.space.lg
            visible: window.currentTab === 0 && !captureResult.hasImage
        }
        ResultPage {
            objectName: "resultPage"
            anchors.fill: parent
            anchors.margins: Tk.Theme.space.lg
            visible: window.currentTab === 0 && captureResult.hasImage
            onSaveAsRequested: window.openSaveAs()
        }
        RecordPage {
            objectName: "recordPage"
            anchors.fill: parent
            anchors.margins: Tk.Theme.space.lg
            visible: window.currentTab === 1
        }
    }

    FileDialog {
        id: saveDialog
        objectName: "saveDialog"
        title: qsTr("Save screenshot as")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)"), qsTr("JPEG image (*.jpg *.jpeg)"), qsTr("WebP image (*.webp)")]
        onAccepted: captureResult.saveAs(selectedFile)
    }

    Instantiator {
        model: captureFlow.phase === "selecting" ? captureFlow.overlayScreens : []
        delegate: RegionOverlay {
            regionState: regionState
        }
    }
}

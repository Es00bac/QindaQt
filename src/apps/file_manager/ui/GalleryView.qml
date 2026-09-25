// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk
import "EntryDrag.js" as EntryDrag
import "EntryText.js" as EntryText
import QindaQt.Controls 1.0 as C

// Gallery (ADR-0270): a large preview of the current entry, its key facts,
// and a Tk.Filmstrip of the folder (GalleryStrip). The large preview comes
// from the bounded pipeline's larger bound ("gallery-previews", main.cpp) and
// is requested only while this view is the one shown. Keyboard, selection,
// drag and drop and the context menu are the window's shared ones; the strip
// has no empty space, so a rubber band has nowhere to start here.
Control {
    id: root
    objectName: "galleryView"

    required property var navigationController
    required property var selection
    required property var appCoordinator
    property var mutationController: null
    property var clipboardController: null
    property var fileActions: null
    property var preferencesController: null
    property var entryFacts: null
    // True while this view is the one shown (FolderViewStack).
    property bool active: false

    property int iconSize: 64
    signal zoomRequested(int steps)

    readonly property bool showExtensions: preferencesController
        ? preferencesController.showExtensions === true : true
    readonly property var current: root.selection.currentIndex >= 0
        ? (root.selection.entries[root.selection.currentIndex] || null) : null

    readonly property alias focusItem: keyTarget
    function focusView() { keyTarget.forceActiveFocus() }
    function currentEntry() { return root.current }
    function selectedEntries() { return root.selection.selectedEntries() }
    function selectAll() { root.selection.selectAll() }
    function activateCurrent() {
        if (root.selection.currentIndex >= 0)
            root.navigationController.activate(root.selection.currentIndex)
    }
    // The strip follows the current entry by itself (GalleryStrip.follow).
    function revealIndex(index) {}
    function popupFor(count) {
        contextMenu.selectionCount = count
        contextMenu.popup()
    }

    ViewportNavigation {
        id: keyboardNavigation
        onQuickLookRequested: root.appCoordinator.activateAction("file.quick-look")
        view: keyTarget
        selection: root.selection
        navigationController: root.navigationController
        columns: 1
        // Page Up/Down move by one strip.
        rowHeight: Math.max(1, keyTarget.height / Math.max(1, strip.count))
        onContextMenuRequested: {
            if (root.selection.currentIndex >= 0) {
                if (root.selection.selectedEntries().length === 0)
                    root.selection.selectOnly(root.selection.currentIndex)
                root.popupFor(root.selection.selectedEntries().length)
            } else {
                root.popupFor(0)
            }
        }
    }

    padding: 8

    contentItem: ColumnLayout {
        spacing: Tk.Theme.space.md

        Item {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true

            DropArea {
                anchors.fill: parent
                onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                onDropped: (drop) => {
                    const folder = root.current && root.current.isDirectory === true
                        ? root.current.path : root.navigationController.currentPath
                    const action = EntryDrag.dispatch(drop, folder, root.mutationController,
                                                      root.clipboardController)
                    if (action !== Qt.IgnoreAction)
                        drop.accept(action)
                    else
                        drop.accepted = false
                }
            }

            // AGENT-CONTRACT: "entryGalleryView" is this view's keyboard
            // focus and tests' handle (selectEntry(), currentIndex, count).
            Item {
                id: keyTarget
                objectName: "entryGalleryView"
                anchors.fill: parent
                focus: true
                readonly property int currentIndex: root.selection.currentIndex
                readonly property int count: root.selection.entries.length
                function selectEntry(index) { root.selection.selectOnly(index) }
                function revealIndex(index) {}
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Folder contents")
                Keys.onReturnPressed: root.activateCurrent()
                Keys.onEnterPressed: root.activateCurrent()
                Keys.onPressed: (event) => keyboardNavigation.handle(event)
            }

            RowLayout {
                anchors.fill: parent
                spacing: Tk.Theme.space.lg

                // The stage: the current entry, as large as the view allows.
                Item {
                    id: stage
                    objectName: "galleryStage"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    readonly property bool previewReady: largePreview.status === Image.Ready
                        && largePreview.implicitWidth > 1
                    readonly property int iconEdge: Math.max(64, Math.min(256, Math.min(width, height) / 2))

                    Drag.active: stageDrag.active
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
                    Drag.mimeData: root.current === null ? ({})
                        : EntryDrag.mimeFor(root.selection.isSelected(root.selection.currentIndex)
                                            ? root.selection.selectedEntries() : [root.current])
                    DragHandler {
                        id: stageDrag
                        target: null
                        enabled: root.current !== null
                    }
                    Accessible.role: Accessible.Graphic
                    Accessible.name: root.current ? root.current.name + EntryText.kindSuffix(root.current) : ""

                    Image {
                        anchors.centerIn: parent
                        width: stage.iconEdge
                        height: stage.iconEdge
                        visible: root.current !== null && !stage.previewReady
                        sourceSize: Qt.size(stage.iconEdge, stage.iconEdge)
                        source: root.current ? EntryText.iconUrl(root.current, stage.iconEdge) : ""
                        Accessible.ignored: true
                    }
                    Image {
                        id: largePreview
                        anchors.fill: parent
                        visible: stage.previewReady
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                        smooth: true
                        source: root.active && root.current ? EntryText.galleryPreviewUrl(root.current) : ""
                        Accessible.ignored: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            root.focusView()
                            if (root.selection.currentIndex < 0) {
                                if (mouse.button === Qt.RightButton)
                                    root.popupFor(0)
                                return
                            }
                            root.selection.click(root.selection.currentIndex, mouse.button, mouse.modifiers)
                            if (mouse.button === Qt.RightButton)
                                root.popupFor(root.selection.selectedEntries().length)
                        }
                        onDoubleClicked: (mouse) => {
                            if (mouse.modifiers === Qt.NoModifier)
                                root.activateCurrent()
                        }
                    }
                    C.TouchContextArea {
                        objectName: "galleryTouchContext"
                        anchors.fill: parent
                        onContextRequested: {
                            root.focusView()
                            if (root.selection.currentIndex >= 0)
                                root.selection.target(root.selection.currentIndex)
                            root.popupFor(root.selection.selectedEntries().length)
                        }
                    }
                }

                EntryFactsPane {
                    objectName: "galleryFacts"
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    visible: root.width >= 560
                    entry: root.current
                    entryFacts: root.entryFacts
                    relativeDates: root.preferencesController
                        ? root.preferencesController.relativeDates === true : false
                    showExtensions: root.showExtensions
                    applicationsPlace: root.navigationController.applicationsPlace === true
                    locale: root.locale
                }
            }
            ZoomWheelHandler { onZoomRequested: (steps) => root.zoomRequested(steps) }
        }

        GalleryStrip {
            id: strip
            objectName: "galleryStrip"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(root.iconSize * 0.75) + 48
            selection: root.selection
            frameSize: Math.round(root.iconSize * 0.75) + 48
            showExtensions: root.showExtensions
            focused: keyTarget.activeFocus
            onClicked: (index, button, modifiers) => {
                root.focusView()
                root.selection.click(index, button, modifiers)
                if (button === Qt.RightButton)
                    root.popupFor(root.selection.selectedEntries().length)
            }
            onDoubleClicked: (index) => {
                root.selection.selectOnly(index)
                root.activateCurrent()
            }
            ZoomWheelHandler { onZoomRequested: (steps) => root.zoomRequested(steps) }
        }

        FileContextMenu {
            id: contextMenu
            objectName: "galleryContextMenu"
            appCoordinator: root.appCoordinator
            navigationController: root.navigationController
            clipboardController: root.clipboardController
            mutationController: root.mutationController
            fileActions: root.fileActions
        }

        Label {
            Layout.fillWidth: true
            visible: root.navigationController.statusMessage.length > 0
            text: root.navigationController.statusMessage
            color: root.palette.placeholderText
        }
    }
}

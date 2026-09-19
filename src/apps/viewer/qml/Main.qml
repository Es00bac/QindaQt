// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import QindaTK as Tk

Tk.AppWindow {
    id: window
    objectName: "viewerWindow"
    width: 960
    height: 680
    minimumWidth: 420
    minimumHeight: 320
    title: viewer.fileName.length > 0 ? viewer.fileName + " — QindaQt Viewer" : qsTr("QindaQt Viewer")
    property bool inWindowMenuVisible: true
    readonly property real rasterRatio: window.screen ? window.screen.devicePixelRatio : 1
    onRasterRatioChanged: renderDelay.restart()
    readonly property var actionList: {
        let actions = []
        for (const menu of coordinator.menus)
            actions = actions.concat(menu.actions)
        return actions
    }

    function dispatch(actionId) {
        switch (actionId) {
        case "file.open": fileDialog.open(); break
        case "file.close": viewer.close(); break
        case "file.quit": window.close(); break
        case "view.previous": viewer.goToPage(viewer.page - 1); break
        case "view.next": viewer.goToPage(viewer.page + 1); break
        case "view.first": viewer.goToPage(0); break
        case "view.last": viewer.goToPage(viewer.pageCount - 1); break
        case "view.zoom-in": viewport.zoomIn(); break
        case "view.zoom-out": viewport.zoomOut(); break
        case "view.actual": viewport.setZoom(1); break
        case "view.fit": viewport.fitPage(); break
        case "view.width": viewport.fitWidth(); break
        case "view.rotate": viewer.rotate(90); break
        }
    }
    Connections {
        target: coordinator
        function onActionRequested(actionId) { window.dispatch(actionId) }
    }
    Connections {
        target: viewer
        function onOpened() { viewport.fitPage(); renderDelay.restart() }
        function onPasswordRequired() { passwordDialog.open() }
        function onStateChanged() {
            pageInput.value = viewer.page + 1
            if (!viewer.locked && !viewer.busy)
                passwordDialog.close()
        }
    }
    Instantiator {
        model: window.actionList
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.shortcut
            enabled: modelData.enabled
            onActivated: coordinator.activateAction(modelData.id)
        }
    }
    Shortcut { sequence: "Ctrl+="; enabled: viewer.ready; onActivated: viewport.zoomIn() }

    menuBar: ViewerMenuBar {
        objectName: "viewerMenuBar"
        visible: window.inWindowMenuVisible
        menusModel: coordinator.menus
        onActivated: actionId => coordinator.activateAction(actionId)
    }
    toolBars: Tk.ToolBar {
        objectName: "viewerToolbar"
        wrap: true
        Tk.Button {
            objectName: "openButton"
            text: qsTr("Open…")
            iconName: "folder-open"
            tooltip: qsTr("Open an image or PDF (Ctrl+O)")
            onClicked: coordinator.activateAction("file.open")
        }
        Tk.ToolSeparator {}
        Tk.IconButton {
            objectName: "previousPage"
            iconName: "chevron-left"
            tooltip: qsTr("Previous page (Page Up)")
            enabled: viewer.ready && viewer.page > 0
            onClicked: viewer.goToPage(viewer.page - 1)
        }
        Tk.NumberField {
            id: pageInput
            objectName: "pageNumber"
            label: qsTr("Page")
            from: 1
            to: Math.max(1, viewer.pageCount)
            enabled: viewer.ready
            implicitWidth: 105
            value: viewer.page + 1
            onValueModified: value => viewer.goToPage(value - 1)
            Accessible.name: qsTr("Page number")
        }
        Tk.Caption { text: qsTr("of %1").arg(viewer.pageCount) }
        Tk.IconButton {
            objectName: "nextPage"
            iconName: "chevron-right"
            tooltip: qsTr("Next page (Page Down)")
            enabled: viewer.ready && viewer.page + 1 < viewer.pageCount
            onClicked: viewer.goToPage(viewer.page + 1)
        }
        Tk.ToolSeparator {}
        Tk.ZoomControl {
            id: zoomControl
            objectName: "zoomControl"
            from: 0.05
            to: 8
            value: viewport.zoom
            enabled: viewer.ready
            onValueModified: value => viewport.setZoom(value)
            onResetRequested: viewport.setZoom(1)
        }
        Tk.Button {
            objectName: "fitPageButton"
            text: qsTr("Fit page")
            enabled: viewer.ready
            tooltip: qsTr("Fit page (Ctrl+1)")
            onClicked: viewport.fitPage()
        }
        Tk.Button {
            text: qsTr("Fit width")
            enabled: viewer.ready
            tooltip: qsTr("Fit width (Ctrl+2)")
            onClicked: viewport.fitWidth()
        }
        Tk.IconButton {
            objectName: "rotateButton"
            iconName: "rotate-cw"
            tooltip: qsTr("Rotate clockwise (Ctrl+R)")
            enabled: viewer.ready
            onClicked: viewer.rotate(90)
        }
    }
    Item {
        Tk.Viewport {
            id: viewport
            objectName: "documentViewport"
            anchors.fill: parent
            visible: viewer.ready
            minZoom: 0.05
            maxZoom: 8
            fitMode: "page"
            onZoomChanged: {
                zoomControl.value = zoom
                renderDelay.restart()
            }
            Item {
                implicitWidth: viewer.pageSize.width
                implicitHeight: viewer.pageSize.height
                Image {
                    objectName: "documentImage"
                    anchors.fill: parent
                    source: viewer.ready ? "image://document/" + viewer.frameRevision : ""
                    cache: false
                    smooth: true
                    Accessible.role: Accessible.Graphic
                    Accessible.name: qsTr("%1, page %2 of %3").arg(viewer.fileName)
                        .arg(viewer.page + 1).arg(viewer.pageCount)
                }
            }
        }
        Tk.EmptyState {
            anchors.fill: parent
            visible: !viewer.ready
            iconName: viewer.locked ? "lock" : "image"
            title: viewer.busy ? qsTr("Opening document…")
                : viewer.locked ? qsTr("Password protected PDF")
                : viewer.error.length > 0 ? qsTr("Unable to open document") : qsTr("Images and PDFs")
            text: viewer.error.length > 0 ? viewer.error
                : qsTr("Open a file or drop an image or PDF here. Use Ctrl+wheel to zoom and the middle mouse button to pan.")
            Tk.Button {
                visible: viewer.locked && !viewer.busy
                text: qsTr("Enter password…")
                onClicked: passwordDialog.open()
            }
            Tk.Button { text: qsTr("Open…"); onClicked: fileDialog.open() }
        }
        DropArea {
            anchors.fill: parent
            onDropped: drop => {
                if (drop.hasUrls && drop.urls.length === 1) {
                    viewer.open(drop.urls[0])
                    drop.acceptProposedAction()
                }
            }
        }
    }
    statusBar: Tk.StatusBar {
        Tk.Caption {
            Tk.Flex.grow: 1
            Tk.Flex.minWidth: 0
            text: viewer.busy ? qsTr("Rendering…") : viewer.ready ? viewer.fileName : qsTr("Ready")
            elide: Text.ElideMiddle
        }
        Tk.Caption {
            visible: viewer.ready
            text: Math.round(viewport.zoom * 100) + "% · " + qsTr("Page %1 / %2")
                .arg(viewer.page + 1).arg(viewer.pageCount)
        }
    }
    Timer {
        id: renderDelay
        interval: 100
        onTriggered: viewer.renderAt(viewport.zoom, window.rasterRatio)
    }
    FileDialog {
        id: fileDialog
        title: qsTr("Open image or PDF")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images and PDFs (*.pdf *.png *.jpg *.jpeg *.gif *.webp *.bmp *.tif *.tiff *.svg)"),
                      qsTr("All files (*)")]
        onAccepted: viewer.open(selectedFile)
    }
    Tk.Dialog {
        id: passwordDialog
        title: qsTr("Unlock PDF")
        subtitle: viewer.error
        primaryText: qsTr("Unlock")
        primaryEnabled: passwordField.text.length > 0
        onOpened: passwordField.forceActiveFocus()
        onAccepted: {
            const password = passwordField.text
            passwordField.text = ""
            viewer.unlock(password)
        }
        onRejected: passwordField.text = ""
        onClosed: passwordField.text = ""
        Tk.TextField {
            id: passwordField
            objectName: "passwordField"
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            Accessible.name: qsTr("PDF password")
        }
    }
}

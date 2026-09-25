// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaTK as Tk
import "EntryText.js" as EntryText

// Quick Look (ADR-0272), Finder's: a Tk.Popover over the window showing the
// current entry large -- a Tk.Thumbnail of its icon, covered by the bounded
// preview pipeline's large picture ("gallery-previews", the Gallery view's)
// once one decodes -- beside its key facts. Space or Escape closes it; the
// arrow keys step through the folder, moving the window's selection, so the
// entry previewed is always the one selected underneath.
//
// AGENT-CONTRACT: opened and closed only through the catalog action
// "file.quick-look" (Main.qml asks handle() first): the File menu, Ctrl+Y,
// Space in every view (ViewportNavigation.quickLookRequested) and a reveal's
// --action from the Desktop (EntryReveal, FileBoundary::revealLocalItem) all
// take that one route. It reads the window's EntrySelection and reaches the
// view the user sees only through FolderViewStack.activeView's hooks
// (revealIndex, focusView), never a view's internals.
Item {
    id: root

    required property var selection
    // The window's FolderViewStack.
    required property var views
    required property var navigationController
    property var entryFacts: null
    property var preferencesController: null

    readonly property bool showing: popover.visible
    readonly property var current: root.selection.currentIndex >= 0
        ? (root.selection.entries[root.selection.currentIndex] || null) : null
    readonly property bool showExtensions: root.preferencesController
        ? root.preferencesController.showExtensions === true : true

    // Returns true when it handled `actionId`.
    function handle(actionId) {
        if (actionId !== "file.quick-look")
            return false
        if (popover.visible)
            popover.close()
        else if (root.current !== null)
            popover.open()
        return true
    }

    // One entry back or forward in the folder's order, selected alone and
    // scrolled into view underneath, as Finder's arrows do.
    function step(delta) {
        const count = root.selection.entries.length
        if (count === 0)
            return
        const index = Math.max(0, Math.min(count - 1, root.selection.currentIndex + delta))
        root.selection.selectOnly(index)
        root.views.activeView.revealIndex(index)
    }

    // Leaving the folder, or losing the entry, ends the preview.
    onCurrentChanged: if (root.current === null) popover.close()
    Connections {
        target: root.navigationController
        function onNavigationChanged() { popover.close() }
    }

    Tk.Popover {
        id: popover
        objectName: "quickLookPopover"

        // Centred in the window rather than beside an anchor (anchorItem
        // stays null, so the Popover's own placement leaves x and y alone).
        width: parent ? Math.min(760, parent.width - 48) : 760
        height: parent ? Math.min(560, parent.height - 48) : 560
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        padding: Tk.Theme.space.md
        focus: true
        onClosed: root.views.activeView.focusView()

        contentItem: Item {
            focus: true
            Accessible.role: Accessible.Dialog
            Accessible.name: root.current ? qsTr("Quick Look: %1").arg(root.current.name)
                                          : qsTr("Quick Look")

            // Escape is the Popover's own close policy.
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Space)
                    popover.close()
                else if (event.key === Qt.Key_Left || event.key === Qt.Key_Up)
                    root.step(-1)
                else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down)
                    root.step(1)
                else
                    return
                event.accepted = true
            }

            RowLayout {
                anchors.fill: parent
                spacing: Tk.Theme.space.lg

                Tk.Thumbnail {
                    id: stage
                    objectName: "quickLookStage"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // The Thumbnail fits its picture to the stage, so the icon
                    // is rendered at the stage's size (capped) to stay sharp.
                    readonly property int iconEdge: Math.max(64, Math.min(512, Math.min(width, height)))
                    readonly property bool previewReady: preview.status === Image.Ready
                        && preview.implicitWidth > 1
                    // Nothing is requested or read while the preview is closed.
                    source: popover.visible && root.current
                        ? EntryText.iconUrl(root.current, stage.iconEdge) : ""
                    placeholderIcon: EntryText.glyphFor(root.current)
                    crop: false
                    tooltip: root.current ? root.current.name : ""

                    // A refused preview arrives as a 1x1 image and leaves the icon.
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: stage.border.width
                        radius: Tk.Theme.radius.sm
                        color: Tk.Theme.color.canvas
                        visible: stage.previewReady
                        Image {
                            id: preview
                            objectName: "quickLookPreview"
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false
                            smooth: true
                            source: popover.visible && root.current
                                ? EntryText.galleryPreviewUrl(root.current) : ""
                            Accessible.ignored: true
                        }
                    }
                }

                EntryFactsPane {
                    objectName: "quickLookFacts"
                    Layout.preferredWidth: 220
                    Layout.fillHeight: true
                    visible: popover.width >= 480
                    entry: popover.visible ? root.current : null
                    entryFacts: root.entryFacts
                    relativeDates: root.preferencesController
                        ? root.preferencesController.relativeDates === true : false
                    showExtensions: root.showExtensions
                    applicationsPlace: root.navigationController.applicationsPlace === true
                }
            }
        }
    }
}

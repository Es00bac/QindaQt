// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "EntryText.js" as EntryText

// The Gallery view's strip (ADR-0270): a Tk.Filmstrip of the folder around
// the current entry. A folder can hold 20,000 entries, so the strip is a
// window of as many frames as fit that follows the current entry (and the
// wheel): only visible frames ever exist.
//
// AGENT-NOTE: Tk.Filmstrip (r5) draws frames but has no selection or
// clicks. It lays frames out evenly and, when it holds no more frames than
// fit, shows every one in order (Filmstrip.qml); so a click maps to a frame
// by position, and the overlays here draw selection and focus.
Item {
    id: root

    required property var selection
    required property int frameSize
    required property bool showExtensions
    property bool focused: false

    signal clicked(int index, int button, int modifiers)
    signal doubleClicked(int index)

    readonly property int gap: Tk.Theme.space.xs
    readonly property int count: Math.max(1, Math.floor((width + gap) / (frameSize + gap)))
    property int start: 0
    readonly property var windowEntries: root.selection.entries.slice(root.start, root.start + root.count)
    readonly property real frameWidth: windowEntries.length > 0
        ? (width - gap * (windowEntries.length - 1)) / windowEntries.length : 0
    property int lastClicked: -1

    // Keeps the window on the folder and the current entry inside it,
    // moving it as little as possible.
    function follow() {
        const last = Math.max(0, root.selection.entries.length - root.count)
        const current = root.selection.currentIndex
        let next = Math.min(root.start, last)
        if (current >= 0 && current < next)
            next = current
        else if (current >= next + root.count)
            next = current - root.count + 1
        root.start = Math.max(0, Math.min(next, last))
    }
    function scrollBy(frames) {
        const last = Math.max(0, root.selection.entries.length - root.count)
        root.start = Math.max(0, Math.min(root.start + frames, last))
    }
    onCountChanged: follow()
    Connections {
        target: root.selection
        function onCurrentIndexChanged() { root.follow() }
        function onEntriesChanged() { root.follow() }
    }

    // An image the preview pipeline can decode shows its preview; anything
    // else shows its theme icon, never a blank frame.
    function frameSource(entry) {
        const previewable = /\.(png|jpe?g|bmp|webp)$/i.test(String(entry.name || ""))
        return previewable && entry.previewUrl ? entry.previewUrl
                                               : EntryText.iconUrl(entry, root.frameSize)
    }
    function indexAt(x) {
        if (root.windowEntries.length === 0)
            return -1
        const slot = Math.floor(x / (root.frameWidth + root.gap))
        return root.start + Math.max(0, Math.min(root.windowEntries.length - 1, slot))
    }

    Tk.Filmstrip {
        anchors.fill: parent
        crop: false
        placeholderIcon: "file"
        minimumFrameWidth: root.frameSize
        frames: root.windowEntries.map(entry => ({
            "source": root.frameSource(entry),
            "caption": EntryText.displayName(entry, root.showExtensions)
        }))
        Accessible.name: qsTr("Items in this folder")
    }

    Repeater {
        model: root.windowEntries.length
        delegate: Rectangle {
            required property int index
            readonly property int entryIndex: root.start + index
            x: index * (root.frameWidth + root.gap)
            width: root.frameWidth
            height: root.height
            radius: Tk.Theme.radius.sm
            color: root.selection.isSelected(entryIndex) ? Tk.Theme.color.selection : "transparent"
            border.width: entryIndex === root.selection.currentIndex
                ? (root.focused ? Tk.Theme.size.focusRing * 2 : Tk.Theme.size.focusRing) : 0
            border.color: root.focused ? Tk.Theme.color.focus : Tk.Theme.color.accent
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            root.lastClicked = root.indexAt(mouse.x)
            if (root.lastClicked >= 0)
                root.clicked(root.lastClicked, mouse.button, mouse.modifiers)
        }
        // The frame the first click chose, even if that click moved the window.
        onDoubleClicked: (mouse) => {
            if (root.lastClicked >= 0 && mouse.modifiers === Qt.NoModifier)
                root.doubleClicked(root.lastClicked)
        }
    }
    WheelHandler {
        acceptedModifiers: Qt.NoModifier
        onWheel: (event) => {
            const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x
            if (delta !== 0)
                root.scrollBy(delta > 0 ? -1 : 1)
        }
    }
}

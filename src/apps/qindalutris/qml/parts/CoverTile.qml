// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// A media tile that is honest about not having a picture yet: the kind icon
// while empty or loading, a muted alert glyph on failure, the caption band
// over the bottom edge, and the selection ring the grid asks for.
//
// AGENT-NOTE: this mirrors Tk.Thumbnail's documented contract. The control
// exists in the QindaTK source tree but is not in the installed
// dev-libs/qindatk-0.1.0-r4 package (the toolkit is another lane's turf),
// so QindaLutris carries the tile locally on Tk.Theme tokens only. Swap back
// to Tk.Thumbnail once the installed toolkit ships it.
Rectangle {
    id: tile

    property url source: ""
    property string placeholderIcon: "image"
    property string caption: ""
    property bool selected: false
    property string tooltip: ""

    readonly property string loadState: tile.source.toString().length === 0
                                    ? "empty"
                                    : image.status === Image.Loading ? "loading"
                                    : image.status === Image.Ready ? "ready"
                                    : image.status === Image.Error ? "failed"
                                    : "loading"

    // AGENT-NOTE: derived from theme roles the INSTALLED toolkit ships --
    // Tk.Theme.size.thumbnail/thumbnailHeight only exist in the unreleased
    // toolkit (see the note above), so the tile is a multiple of iconXl.
    readonly property real naturalWidth: Tk.Theme.size.iconXl * 8

    objectName: "coverTile"
    implicitWidth: naturalWidth
    implicitHeight: Math.round(naturalWidth * 1.4)
    radius: Tk.Theme.radius.sm
    color: Tk.Theme.color.canvas
    border.width: tile.selected ? Tk.Theme.size.focusRing : Tk.Theme.size.border
    border.color: tile.selected ? Tk.Theme.color.accent : Tk.Theme.color.border
    clip: true

    Accessible.role: Accessible.Graphic
    Accessible.name: tile.tooltip.length > 0 ? tile.tooltip : tile.caption

    Tk.Icon {
        objectName: "coverTilePlaceholder"
        anchors.centerIn: parent
        name: tile.loadState === "failed" ? "alert-triangle" : tile.placeholderIcon
        size: Tk.Theme.size.iconXl
        color: tile.loadState === "failed" ? Tk.Theme.color.warning
                                           : Tk.Theme.color.textDisabled
        visible: tile.loadState !== "ready"
    }

    Image {
        id: image
        objectName: "coverTileImage"
        anchors.fill: parent
        anchors.margins: tile.border.width
        source: tile.source
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        visible: tile.loadState === "ready"
    }

    Rectangle {
        objectName: "coverTileCaption"
        visible: tile.caption.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        implicitHeight: bandText.implicitHeight + Tk.Theme.space.xs * 2
        color: Tk.Theme.color.panel
        opacity: Tk.Theme.opacity.muted

        Tk.Caption {
            id: bandText
            anchors.centerIn: parent
            text: tile.caption
            elide: Text.ElideRight
        }
    }
}

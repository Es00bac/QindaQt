// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// The cover flow: one wrapping row of tiles, each a Tk.Thumbnail with the
// game's title beneath. Covers come from image://gameicon/<game id>; a game
// without art keeps the toolkit's placeholder glyph rather than a hole.
Item {
    id: grid

    required property var model
    property string selectedGameId: ""

    signal selectRequested(string gameId)
    signal playRequested()

    Tk.Scroll {
        anchors.fill: parent
        overflowX: Tk.Scroll.Hidden

        Tk.Flex {
            direction: Tk.Flex.Row
            wrap: Tk.Flex.Wrap
            gap: Tk.Theme.space.md
            padding: Tk.Theme.space.xs
            width: grid.width

            Repeater {
                model: grid.model

                delegate: Item {
                    id: tile
                    required property string gameId
                    required property string title
                    required property string sourceLabel
                    required property url coverUrl

                    readonly property real tileWidth: Tk.Theme.size.iconXl * 8

                    // AGENT-NOTE: Tk.Flex measures children by their IMPLICIT
                    // size; a plain Item that only sets width/height measures
                    // 0x0 and piles at the origin (QindaTK layout rule 5).
                    implicitWidth: tile.tileWidth
                    implicitHeight: column.implicitHeight
                    width: tile.tileWidth
                    height: column.implicitHeight
                    Tk.Flex.shrink: 0

                    Accessible.role: Accessible.Button
                    Accessible.name: tile.title + ", " + tile.sourceLabel

                    Tk.Flex {
                        id: column
                        direction: Tk.Flex.Column
                        gap: Tk.Theme.space.xs
                        width: tile.tileWidth

                        CoverTile {
                            objectName: "cover-" + tile.gameId
                            source: tile.coverUrl
                            caption: tile.sourceLabel
                            selected: grid.selectedGameId === tile.gameId
                            tooltip: tile.title
                            placeholderIcon: "gamepad-2"
                        }
                        Tk.Caption {
                            objectName: "title-" + tile.gameId
                            text: tile.title
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            Tk.Flex.alignSelf: Tk.Flex.Stretch
                        }
                    }

                    TapHandler {
                        onTapped: grid.selectRequested(tile.gameId)
                        onDoubleTapped: {
                            grid.selectRequested(tile.gameId)
                            grid.playRequested()
                        }
                    }
                }
            }
        }
    }
}

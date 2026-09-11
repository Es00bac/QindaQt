// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Right column of the start panel: the Places inventory rendered as the
// XP-blue link panel beside the white program list.
//
// AGENT-NOTE: rows are the PlacesController::rows() projection
// ({id, label, path, iconName, accessibleName, index}; the "computer" row is
// part of the inventory). Opening re-enters places.open(id), the facade's
// single bounded process seam; the controller fails closed (no opener, no
// grant → false) and this column then keeps the panel open, mirroring
// PlacesMenuApplet.
Rectangle {
    id: root

    required property var places
    readonly property bool ready: places !== null && Tokens.ready

    signal placeOpened()

    function focusRow(index) {
        if (index < 0 || index >= rowRepeater.count)
            return
        const item = rowRepeater.itemAt(index)
        if (item !== null)
            item.forceActiveFocus(Qt.PopupFocusReason)
    }

    objectName: "startMenuPlacesColumn"
    // Luna dressing constants (ADR-0124): the XP-blue link panel with its
    // divider. The dark text is the fixed contrast pair of this surface, so
    // it cannot follow the theme without breaking the dressing.
    color: "#d3e5fa"

    C.Label {
        objectName: "startMenuPlacesUnavailable"
        anchors.centerIn: parent
        width: parent.width - 12
        visible: !root.ready
        text: qsTr("Places are unavailable")
        muted: true
    }

    T.ScrollView {
        id: placesView

        objectName: "startMenuPlaces"
        anchors.fill: parent
        anchors.topMargin: 6
        visible: root.ready
        contentWidth: availableWidth
        clip: true
        focusPolicy: Qt.NoFocus

        ColumnLayout {
            id: listColumn

            width: placesView.availableWidth
            spacing: 0

            Repeater {
                id: rowRepeater

                model: root.ready ? root.places.rows : []

                delegate: T.ItemDelegate {
                    id: row

                    required property var modelData
                    required property int index

                    objectName: "startMenuPlaceRow"
                    Layout.fillWidth: true
                    enabled: root.ready
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    implicitHeight: 30
                    leftPadding: 8
                    rightPadding: 4
                    Accessible.role: Accessible.ListItem
                    Accessible.name: String(modelData.accessibleName)
                    Accessible.description: String(modelData.path)

                    function openPlace() {
                        if (root.ready
                                && root.places.open(String(modelData.id)))
                            root.placeOpened()
                    }

                    onClicked: openPlace()
                    Keys.onReturnPressed: openPlace()
                    Keys.onEnterPressed: openPlace()
                    Keys.onSpacePressed: openPlace()
                    Keys.onUpPressed: root.focusRow(index - 1)
                    Keys.onDownPressed: root.focusRow(index + 1)
                    Accessible.onPressAction: openPlace()

                    background: Rectangle {
                        objectName: "startMenuPlaceRowBackground"
                        radius: 2
                        color: row.hovered || row.down ? "#91c9f7"
                                                       : "transparent"

                        C.FocusRing {
                            anchors.fill: parent
                            control: row
                        }
                    }

                    contentItem: Row {
                        spacing: 6

                        ShellIcons.Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: String(row.modelData.iconName ?? "")
                            size: 16
                            fallbackText: row.modelData.label
                            Accessible.ignored: true
                        }

                        C.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - parent.spacing - 16
                            text: row.modelData.label
                            color: "#1c2a45"
                            wrapMode: Text.NoWrap
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // Luna dressing constant: the column divider against the white body.
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: "#6a8bcc"
        Accessible.ignored: true
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The area editor of a desk tablet (ADR-0285): the tablet as it lies on the
// desk beside the screen it reaches, each with the rectangle that maps onto
// the other. "Keep proportions" locks the screen rectangle to the tablet
// rectangle's physical shape, so a circle drawn on the tablet stays round.
//
// AGENT-CONTRACT: rectangles here are in the frames the user SEES (the tablet
// turned the way it lies, the screen as it appears). The placement model
// converts them for KWin; this file never rotates anything itself.
ColumnLayout {
    id: root

    required property var placement

    readonly property Item firstFocusTarget: tabletCanvas.visible ? tabletCanvas : screenCanvas
    readonly property var inputRect: root.placement !== null
                                     && root.placement.inputArea.length === 4
                                     ? root.placement.inputArea : [0, 0, 1, 1]
    readonly property var outputRect: root.placement !== null
                                      && root.placement.outputArea.length === 4
                                      ? root.placement.outputArea : [0, 0, 1, 1]

    spacing: Tokens.space["2"]

    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["4"]

        ColumnLayout {
            visible: root.placement !== null && root.placement.inputAreaAvailable
            spacing: Tokens.space["1"]

            TabletAreaCanvas {
                id: tabletCanvas
                objectName: "tabletInputCanvas"
                Layout.preferredWidth: 240
                Layout.preferredHeight: 170
                surfaceAspect: root.placement !== null && root.placement.tabletWidth > 0
                               && root.placement.tabletHeight > 0
                               ? root.placement.tabletWidth / root.placement.tabletHeight : 1.6
                areaX: root.inputRect[0]
                areaY: root.inputRect[1]
                areaWidth: root.inputRect[2]
                areaHeight: root.inputRect[3]
                surfaceName: qsTr("Part of the tablet the pen uses")
                onCommitted: (x, y, width, height) =>
                    root.placement.applyInputArea(x, y, width, height)
            }

            Label {
                Layout.preferredWidth: 240
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Tablet, as it lies on the desk")
                muted: true
                font.pointSize: Tokens.type.caption
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }
        }

        ColumnLayout {
            visible: root.placement !== null && root.placement.outputAreaAvailable
            spacing: Tokens.space["1"]

            TabletAreaCanvas {
                id: screenCanvas
                objectName: "tabletOutputCanvas"
                Layout.preferredWidth: 280
                Layout.preferredHeight: 170
                surfaceAspect: root.placement !== null && root.placement.surfaceWidth > 0
                               && root.placement.surfaceHeight > 0
                               ? root.placement.surfaceWidth / root.placement.surfaceHeight
                               : 16 / 9
                // A single screen needs no outline; a workspace shows each.
                outlines: root.placement !== null && root.placement.surfaceScreens.length > 1
                          ? root.placement.surfaceScreens : []
                lockedAspect: root.placement !== null ? root.placement.outputAspectLock : 0
                areaX: root.outputRect[0]
                areaY: root.outputRect[1]
                areaWidth: root.outputRect[2]
                areaHeight: root.outputRect[3]
                surfaceName: root.placement !== null
                             ? qsTr("Part of %1 the pen reaches").arg(root.placement.surfaceLabel)
                             : ""
                onCommitted: (x, y, width, height) =>
                    root.placement.applyOutputArea(x, y, width, height)
            }

            Label {
                objectName: "tabletOutputCanvasCaption"
                Layout.preferredWidth: 280
                horizontalAlignment: Text.AlignHCenter
                text: root.placement !== null ? root.placement.surfaceLabel : ""
                muted: true
                font.pointSize: Tokens.type.caption
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["3"]

        Switch {
            id: keepSwitch
            objectName: "tabletKeepProportions"
            visible: root.placement !== null && root.placement.proportionsAvailable
            text: qsTr("Keep proportions")
            accessibleDescription: qsTr("Shape the screen area like the tablet area, so circles stay round")
            onToggled: root.placement.keepProportions = checked
        }

        Button {
            objectName: "tabletAreaReset"
            text: qsTr("Reset areas")
            emphasized: false
            accessibleDescription: qsTr("Use the whole tablet on the whole screen again")
            onClicked: root.placement.resetAreas()
        }
    }

    // AGENT-GUARD: a Switch's own toggle breaks a plain `checked:` binding,
    // and a refused change would then leave it showing a state the model
    // never took. The Binding element re-asserts the model's truth.
    Binding {
        target: keepSwitch
        property: "checked"
        value: root.placement !== null && root.placement.keepProportions
        restoreMode: Binding.RestoreBindingOrValue
    }
}

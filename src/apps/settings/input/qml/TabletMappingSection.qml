// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Where the pen draws: which screen it covers and how much of it.
//
// AGENT-GUARD: "Map to" and the screen picker are one decision. Both go
// through the selection's applyMapping(), never through two writes of their
// own, or the pen spends a frame on the wrong screen.
ColumnLayout {
    id: root

    required property var selection
    readonly property bool mapsToOutput: root.selection !== null
                                         && root.selection.mapMode === "output"
    readonly property var area: root.selection !== null
                                && root.selection.outputArea.length === 4
                                ? root.selection.outputArea : [0, 0, 1, 1]
    readonly property Item firstFocusTarget: mapRow

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Where the pen draws")
        description: qsTr("Which screen the tablet surface covers")
    }

    FormRow {
        id: mapRow
        objectName: "tabletMapRow"
        Layout.fillWidth: true
        label: qsTr("Map to")
        description: qsTr("A pen display should point at its own screen")
        editor: ComboBox {
            objectName: "tabletMapCombo"
            width: 320
            model: [qsTr("The active screen"), qsTr("A specific screen"),
                    qsTr("The whole workspace")]
            currentIndex: root.selection === null ? 0
                          : root.selection.mapMode === "output" ? 1
                          : root.selection.mapMode === "workspace" ? 2 : 0
            onActivated: index => {
                const mode = index === 1 ? "output"
                           : index === 2 ? "workspace" : "follow"
                const names = root.selection.outputNames
                const output = index !== 1 ? ""
                    : (root.selection.outputName.length > 0
                       ? root.selection.outputName
                       : (names.length > 0 ? names[0] : ""))
                root.selection.applyMapping(mode, output)
            }
        }
    }

    FormRow {
        objectName: "tabletOutputRow"
        Layout.fillWidth: true
        visible: root.mapsToOutput
        label: qsTr("Screen")
        description: qsTr("The screen the pen's surface covers")
        editor: ComboBox {
            objectName: "tabletOutputCombo"
            width: 320
            model: root.selection !== null ? root.selection.outputLabels : []
            currentIndex: root.selection === null ? -1
                          : root.selection.outputNames.indexOf(
                                root.selection.outputName)
            onActivated: index => {
                const names = root.selection.outputNames
                if (index >= 0 && index < names.length)
                    root.selection.applyMapping("output", names[index])
            }
        }
    }

    TabletMapDiagram {
        objectName: "tabletMapDiagram"
        Layout.fillWidth: true
        Layout.preferredHeight: 170
        visible: root.selection !== null && root.selection.outputAreaAvailable
        areaX: root.area[0]
        areaY: root.area[1]
        areaWidth: root.area[2]
        areaHeight: root.area[3]
        caption: root.selection === null ? ""
                 : root.selection.mapMode === "workspace"
                   ? qsTr("The pen reaches every screen.")
                   : root.selection.mapMode === "output"
                     ? qsTr("The pen reaches %1.").arg(root.selection.outputName)
                     : qsTr("The pen follows whichever screen is active.")
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.outputAreaAvailable
        spacing: Tokens.space["2"]

        Button {
            objectName: "tabletAreaWholeScreen"
            text: qsTr("Fit the whole screen")
            emphasized: false
            accessibleDescription: qsTr("Stretch the tablet surface over the entire screen")
            onClicked: root.selection.fitWholeScreen()
        }

        Button {
            objectName: "tabletAreaKeepProportions"
            text: qsTr("Keep the tablet's proportions")
            emphasized: false
            available: root.selection !== null && root.selection.aspectRatioAvailable
            accessibleDescription: qsTr("Letterbox the mapped area so a square on the tablet is a square on screen")
            onClicked: {
                // The screen the settings window is on is the best proxy the
                // route has for the mapped screen's shape; the area is
                // normalized, so only the ratio matters.
                const screen = root.Window.window !== null
                    ? root.Window.window.screen : null
                root.selection.keepTabletProportions(
                    screen !== null ? screen.width : 16,
                    screen !== null ? screen.height : 9)
            }
        }
    }
}

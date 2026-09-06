// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.kwin as KWin

// KWin remains the focus and activation authority. Its model already consumes
// each window's skipSwitcher flag, so a QindaQt container arrives here as one
// current-page representative under that representative's native caption.
KWin.TabBoxSwitcher {
    id: tabBox

    currentIndex: switcherFrame.currentIndex

    Timer {
        id: activationTimer

        property int selectedIndex: -1
        interval: Kirigami.Units.shortDuration
        onTriggered: {
            if (selectedIndex >= 0)
                tabBox.model.activate(selectedIndex)
        }
    }

    PlasmaCore.Dialog {
        visible: tabBox.visible
        flags: Qt.Popup | Qt.FramelessWindowHint | Qt.X11BypassWindowManagerHint
        backgroundHints: PlasmaCore.Dialog.NoBackground
        x: Math.round(tabBox.screenGeometry.x
                      + (tabBox.screenGeometry.width - switcherFrame.width) / 2)
        y: Math.round(tabBox.screenGeometry.y
                      + (tabBox.screenGeometry.height - switcherFrame.height) / 2)

        mainItem: QindaQtSwitcherFrame {
            id: switcherFrame

            nativeModel: tabBox.model
            screenGeometry: tabBox.screenGeometry
            onActivateRequested: index => {
                activationTimer.selectedIndex = index
                activationTimer.restart()
            }
        }

        onSceneGraphError: () => {
            // KWin owns recovery from a graphics reset; avoid terminating its
            // process because this presentation window lost its scene graph.
        }
    }

    Connections {
        target: tabBox

        function onCurrentIndexChanged() {
            switcherFrame.select(tabBox.currentIndex)
        }
    }
}

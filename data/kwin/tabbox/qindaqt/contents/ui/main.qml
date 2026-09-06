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
            if (tabBox.visible && tabBox.model !== null
                    && selectedIndex >= 0
                    && selectedIndex < tabBox.model.rowCount()) {
                tabBox.model.activate(selectedIndex)
            }
            selectedIndex = -1
        }
    }

    onVisibleChanged: {
        if (!visible) {
            // A mouse click is delayed briefly for selection feedback. Alt
            // release or a model withdrawal can close the popup first; never
            // let that stale callback activate a window after switching ended.
            activationTimer.stop()
            activationTimer.selectedIndex = -1
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

// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    property var navigation: null
    signal closeRequested()
    readonly property bool dirty: CustomizeRouteComposition.model.dirty
    readonly property Item firstFocusTarget: page.firstFocusTarget

    Connections {
        target: root.navigation
        function onActiveRouteChanged() {
            if (root.navigation.activeRouteComponent !== "customize"
                    && root.dirty) {
                page.requestClose()
            }
        }
    }

    CustomizePage {
        id: page
        anchors.fill: parent
        customizeSettings: CustomizeRouteComposition.model
        onCloseRequested: {
            root.closeRequested()
            if (root.navigation === null
                    || root.navigation.activeRouteComponent === "customize") {
                Qt.quit()
            }
        }
        onCloseCancelled: {
            if (root.navigation !== null
                    && root.navigation.activeRouteComponent !== "customize") {
                root.navigation.selectRoute("customize")
            }
        }
    }
}

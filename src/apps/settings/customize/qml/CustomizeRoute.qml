// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    required property var customizeSettings
    property var navigation: null
    signal closeRequested()
    signal closeCancelled()
    readonly property bool dirty: root.customizeSettings.dirty
    readonly property Item firstFocusTarget: page.firstFocusTarget

    function requestClose() {
        page.requestClose()
    }

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
        customizeSettings: root.customizeSettings
        onCloseRequested: {
            root.closeRequested()
            if (root.navigation === null
                    || root.navigation.activeRouteComponent === "customize") {
                Qt.quit()
            }
        }
        onCloseCancelled: {
            root.closeCancelled()
            if (root.navigation !== null
                    && root.navigation.activeRouteComponent !== "customize") {
                root.navigation.selectRoute("customize")
            }
        }
    }
}

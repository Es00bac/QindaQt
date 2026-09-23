// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Reports only the active route Loader after it has an actual page/diagnostic
// item. This is a construction witness for the executable's private probe;
// page behavior and service availability remain route-owned.
Item {
    id: witness
    visible: false
    width: 0
    height: 0

    required property var navigation
    required property Loader currentLoader
    required property Loader diagnosticLoader
    required property bool presentationActive
    signal constructed(string routeId, string state)

    function report() {
        if (!presentationActive || !navigation || !currentLoader
                || !currentLoader.active || currentLoader.status !== Loader.Ready
                || currentLoader.item === null) {
            return
        }
        const routeId = navigation.activeRouteId
        if (!routeId || routeId.length === 0) {
            return
        }
        // AGENT-GUARD: a registered unavailable route must render the
        // diagnostic Loader; a missing page component must not masquerade as
        // intentional unavailability.
        if (!navigation.activeRouteAvailable) {
            if (currentLoader === diagnosticLoader) {
                constructed(routeId, "diagnosed-unavailable")
            }
        } else if (currentLoader !== diagnosticLoader) {
            constructed(routeId, "ready")
        }
    }

    Connections {
        target: witness.currentLoader
        function onLoaded() { Qt.callLater(witness.report) }
        function onStatusChanged() { Qt.callLater(witness.report) }
    }
    onCurrentLoaderChanged: Qt.callLater(report)
    onPresentationActiveChanged: Qt.callLater(report)
    Component.onCompleted: Qt.callLater(report)
}

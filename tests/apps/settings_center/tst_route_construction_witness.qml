// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/apps/settings_center"

TestCase {
    name: "RouteConstructionWitness"
    when: true

    QtObject {
        id: navigation
        property string activeRouteId: "appearance"
        property bool activeRouteAvailable: true
    }
    Loader {
        id: diagnosticLoader
        active: true
        sourceComponent: Item {}
    }
    Loader {
        id: pageLoader
        active: true
        sourceComponent: Item {}
    }
    SettingsRouteConstructionWitness {
        id: witness
        navigation: navigation
        currentLoader: diagnosticLoader
        diagnosticLoader: diagnosticLoader
        presentationActive: false
        onConstructed: (routeId, state) => {
            observedRoute = routeId
            observedState = state
            observedCount++
        }
    }
    property int observedCount: 0
    property string observedRoute: ""
    property string observedState: ""

    function test_availableRouteCannotPassThroughDiagnosticLoader() {
        compare(diagnosticLoader.status, Loader.Ready)
        compare(pageLoader.status, Loader.Ready)
        witness.presentationActive = true
        wait(0)
        compare(observedCount, 0)

        witness.currentLoader = pageLoader
        wait(0)
        compare(observedCount, 1)
        compare(observedRoute, "appearance")
        compare(observedState, "ready")

        navigation.activeRouteAvailable = false
        witness.currentLoader = diagnosticLoader
        wait(0)
        compare(observedCount, 2)
        compare(observedState, "diagnosed-unavailable")
    }
}

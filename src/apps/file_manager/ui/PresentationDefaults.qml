// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Applies the durable presentation preferences (ADR-0198) to the window's
// navigation controller: once when the window opens, and again whenever the
// user changes one in Preferences, so a change is visible without a restart.
//
// AGENT-NOTE: this is the only place the two meet. PreferencesController
// deliberately knows nothing about NavigationController, and vice versa;
// binding them here keeps both free of a dependency on the other.
QtObject {
    id: root

    required property var preferencesController
    required property var navigationController

    // AGENT-GUARD: idempotent. Every step is skipped when the controller
    // already holds the wanted value, because setSortColumn() *flips* the
    // direction when called with the column that is already active (the
    // documented header-click behaviour) and because a needless re-sort
    // resets the view's current index under the user's selection.
    function apply() {
        const navigation = root.navigationController
        const preferences = root.preferencesController
        if (navigation.viewMode !== preferences.defaultViewMode)
            navigation.setViewMode(preferences.defaultViewMode)
        if (navigation.showHidden !== preferences.showHidden)
            navigation.setShowHidden(preferences.showHidden)
        if (navigation.directoriesFirst !== preferences.directoriesFirst)
            navigation.setDirectoriesFirst(preferences.directoriesFirst)
        if (navigation.sortColumn !== preferences.sortColumn)
            navigation.setSortColumn(preferences.sortColumn)
        // A column change lands on ascending, so the direction is corrected
        // afterwards by asking for the same column once more.
        if (navigation.sortDirection !== preferences.sortDirection)
            navigation.setSortColumn(preferences.sortColumn)
        // The zoom ladder is NavigationController's; walking it with the
        // public controls reaches any rung in at most six bounded steps.
        let guard = 0
        while (navigation.iconSize < preferences.iconSize && navigation.canZoomIn && guard++ < 8)
            navigation.zoomBy(1)
        while (navigation.iconSize > preferences.iconSize && navigation.canZoomOut && guard++ < 8)
            navigation.zoomBy(-1)
    }

    property Connections preferenceChanges: Connections {
        target: root.preferencesController
        function onPreferencesChanged() { root.apply() }
    }

    Component.onCompleted: root.apply()
}

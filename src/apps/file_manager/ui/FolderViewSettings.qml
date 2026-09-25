// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// What each folder looks like (ADR-0270, over ADR-0198's store), as Finder
// does it: a folder the user gave a view of its own -- by changing its view,
// order, grouping, zoom or Details columns while in it -- opens in that view;
// any other folder keeps the view the window already shows. The defaults are
// what a window starts with, and they reach the open window whenever they
// change (Preferences, Use as Defaults, Restore Defaults) unless its folder
// has a view of its own. This object also applies the window-wide Show
// Hidden Files and Folders First preferences, as PresentationDefaults did.
//
// AGENT-NOTE: this is the only place PreferencesController and
// NavigationController meet; neither knows the other.
//
// AGENT-GUARD: a change is remembered only for the location this object last
// settled, and never while it applies a view. NavigationController reports a
// new folder (and ApplicationsPlaceOrder re-sorts, ADR-0262) before this
// object hears navigationChanged, so any other rule would file one folder's
// view under another's name.
QtObject {
    id: root

    required property var preferencesController
    required property var navigationController

    // The Details columns this window shows: [{key, width}], Name first.
    property var columns: []
    property string settledLocation: ""
    property string appliedView: ""
    property string lastDefaults: ""
    property bool applying: false
    // Set by the Columns view before it moves between folder levels: a
    // column walk stays in Columns even into a folder with its own view.
    property bool keepViewModeOnce: false
    property var lastShowHidden: undefined
    property var lastDirectoriesFirst: undefined
    property string pendingLocation: ""
    property var pendingView: null

    // The window's view as the store spells it. ADR-0262: the Applications
    // place's order belongs to ApplicationsPlaceOrder and it never groups, so
    // only its view, zoom and columns are its own.
    function currentView() {
        const navigation = root.navigationController
        const preferences = root.preferencesController
        const applications = navigation.applicationsPlace === true
        return {
            "viewMode": navigation.viewMode,
            "sortColumn": applications ? preferences.sortColumn : navigation.sortColumn,
            "sortDirection": applications ? preferences.sortDirection : navigation.sortDirection,
            "groupBy": applications ? preferences.groupBy : navigation.groupBy,
            "iconSize": navigation.iconSize,
            "columns": root.columns
        }
    }

    // AGENT-GUARD: idempotent. Every step is skipped when the controller
    // already holds the wanted value, because setSortColumn() *reverses* the
    // active column (the header-click rule) and a needless re-sort resets
    // the view's current index under the user's selection.
    function applyView(view) {
        const navigation = root.navigationController
        root.applying = true
        if (navigation.viewMode !== view.viewMode)
            navigation.setViewMode(view.viewMode)
        if (navigation.applicationsPlace !== true) {
            if (navigation.groupBy !== view.groupBy)
                navigation.setGroupBy(view.groupBy)
            // A column change lands on ascending; one more call reverses it.
            if (navigation.sortColumn !== view.sortColumn)
                navigation.setSortColumn(view.sortColumn)
            if (navigation.sortDirection !== view.sortDirection)
                navigation.setSortColumn(view.sortColumn)
        }
        // The zoom ladder is NavigationController's; walking it with the
        // public controls reaches any rung in at most six bounded steps.
        let guard = 0
        while (navigation.iconSize < view.iconSize && navigation.canZoomIn && guard++ < 8)
            navigation.zoomBy(1)
        while (navigation.iconSize > view.iconSize && navigation.canZoomOut && guard++ < 8)
            navigation.zoomBy(-1)
        // Reassigning equal columns would rebuild every Details cell.
        if (JSON.stringify(root.columns) !== JSON.stringify(view.columns))
            root.columns = view.columns
        root.appliedView = JSON.stringify(view)
        root.applying = false
    }

    function applyWindowWide() {
        const navigation = root.navigationController
        const preferences = root.preferencesController
        // These follow only their own changes, so a session toggle (Ctrl+H)
        // survives an unrelated preference write.
        if (root.lastShowHidden !== preferences.showHidden) {
            root.lastShowHidden = preferences.showHidden
            if (navigation.showHidden !== preferences.showHidden)
                navigation.setShowHidden(preferences.showHidden)
        }
        if (root.lastDirectoriesFirst !== preferences.directoriesFirst) {
            root.lastDirectoriesFirst = preferences.directoriesFirst
            if (navigation.directoriesFirst !== preferences.directoriesFirst)
                navigation.setDirectoriesFirst(preferences.directoriesFirst)
        }
    }

    // Brings the window to its folder's view after a move (`moved`) or a
    // preference change.
    function settle(moved) {
        const navigation = root.navigationController
        const preferences = root.preferencesController
        const location = navigation.currentPath
        // A preference change heard mid-move (the move's own flush) waits:
        // onNavigationChanged settles the new folder right after.
        if (!moved && root.settledLocation.length > 0 && location !== root.settledLocation)
            return
        root.applyWindowWide()
        let view = preferences.folderView(location)
        const defaults = JSON.stringify(preferences.defaultFolderView())
        const defaultsChanged = defaults !== root.lastDefaults
        root.lastDefaults = defaults
        if (root.keepViewModeOnce && moved) {
            root.keepViewModeOnce = false
            view = Object.assign({}, view, { "viewMode": navigation.viewMode })
        }
        const firstTime = root.settledLocation.length === 0
        if (view.remembered === true) {
            if (moved || JSON.stringify(view) !== root.appliedView)
                root.applyView(view)
        } else if (firstTime || defaultsChanged) {
            root.applyView(view)
        }
        // ADR-0262: Applications groups by category, never by Group By.
        if (navigation.applicationsPlace === true && navigation.groupBy !== "none") {
            root.applying = true
            navigation.setGroupBy("none")
            root.applying = false
        }
        root.settledLocation = location
    }

    function noteChange() {
        const navigation = root.navigationController
        if (root.applying || root.settledLocation.length === 0
                || navigation.currentPath !== root.settledLocation
                || navigation.folderViewActive === false)
            return
        root.pendingLocation = root.settledLocation
        root.pendingView = root.currentView()
        root.rememberTimer.restart()
    }

    function flush() {
        root.rememberTimer.stop()
        if (root.pendingView === null)
            return
        const view = root.pendingView
        root.pendingView = null
        root.preferencesController.rememberFolderView(root.pendingLocation, view)
    }

    // The Details view's column edits (DetailsColumnSet.edit).
    function setColumns(next) {
        root.columns = next
        root.noteChange()
    }

    // View ▸ Use as Defaults: this folder's view becomes every other
    // folder's starting point.
    function useAsDefaults() {
        root.flush()
        root.preferencesController.useAsDefaults(root.currentView())
    }

    // A burst of changes (a wheel zoom, a seam drag) is written once.
    property Timer rememberTimer: Timer {
        interval: 400
        onTriggered: root.flush()
    }

    property Connections navigationChanges: Connections {
        target: root.navigationController
        function onNavigationChanged() {
            root.flush()
            root.settle(true)
        }
        function onPresentationChanged() { root.noteChange() }
    }

    property Connections preferenceChanges: Connections {
        target: root.preferencesController
        function onPreferencesChanged() { root.settle(false) }
    }

    Component.onCompleted: root.settle(true)
    // A change made just before the window closes is kept too; nothing is
    // applied to a window that is going away.
    Component.onDestruction: {
        root.navigationChanges.enabled = false
        root.preferenceChanges.enabled = false
        root.flush()
    }
}

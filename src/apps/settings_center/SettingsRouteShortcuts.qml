// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The Ctrl+digit route shortcuts.
//
// AGENT-CONTRACT: exactly the first ten registered routes get a digit, in
// registration order, Ctrl+1 through Ctrl+9 and then Ctrl+0 for the tenth.
// Routes appended after those ten deliberately have no digit (ADR-0128) and
// are reached from the sidebar or the compact tab list, so adding a route
// never moves an existing shortcut.
Item {
    id: root

    required property var navigation

    readonly property int digitRouteCount:
        root.navigation ? Math.min(10, root.navigation.routeCount) : 0

    Repeater {
        model: root.digitRouteCount

        delegate: Item {
            id: shortcutHolder
            required property int index

            Shortcut {
                // Ctrl+1..Ctrl+9, then Ctrl+0 for the tenth route.
                sequence: "Ctrl+" + ((shortcutHolder.index + 1) % 10)
                onActivated: root.navigation.selectIndex(shortcutHolder.index)
            }
        }
    }
}

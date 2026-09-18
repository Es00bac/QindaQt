// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.SettingsApp.DateTime

// AGENT-CONTRACT: route page `Component`s that would otherwise be declared in
// Main.qml. That file is already over its source-shape limit, and every new
// route has been pushing it further, so routes added from here on declare
// their component here and Main.qml gains one binding instead of a block.
//
// The pre-existing blocks in Main.qml are deliberately left where they are:
// several wave lanes are editing that file concurrently, and moving them now
// would turn their textual merges into manual ones. They belong here, and
// moving them is worth its own small change once those lanes have landed.
QtObject {
    id: root

    // The Date & time route (ADR-0200). Its model comes from the module's own
    // composition singleton, so the window passes nothing in.
    readonly property Component dateTime: Component {
        DateTimePage {
            objectName: "dateTimePage"
            dateTimeSettings: DateTimeRouteComposition.model
            onCloseRequested: root.closeRequested()
        }
    }

    signal closeRequested()
}

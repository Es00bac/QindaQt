// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton
import QtQuick

QtObject {
    readonly property bool ready: true
    readonly property var space: ({ "1": 2, "2": 4, "3": 8, "4": 12 })
    readonly property var fg: ({
        "default": "#f0f4f1",
        "muted": "#aeb8b2",
        "disabled": "#748079"
    })
}

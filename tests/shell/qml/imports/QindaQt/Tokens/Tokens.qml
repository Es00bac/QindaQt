// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton
import QtQuick

QtObject {
    readonly property bool ready: true
    readonly property var space: ({
        "1": 2, "2": 4, "3": 8, "4": 12, "5": 16, "6": 24
    })
    readonly property var fg: ({
        "default": "#f0f4f1",
        "muted": "#aeb8b2",
        "disabled": "#748079"
    })
    readonly property var bg: ({
        "base": "#171a18",
        "raised": "#2c312e",
        "highest": "#3c433f"
    })
    readonly property var state: ({
        "hover": "#31483f",
        "pressed": "#26382f"
    })
    readonly property var radius: ({
        "s": 4,
        "m": 8,
        "l": 12
    })
    readonly property var outline: ({
        "divider": "#3c433f",
        "strong": "#f0f4f1"
    })
    readonly property var accessibility: ({
        "reducedMotion": false,
        "reducedTransparency": false,
        "highContrast": false
    })
}

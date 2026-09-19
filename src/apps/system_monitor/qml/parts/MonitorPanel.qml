// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// AGENT-CONTRACT: the frame every dashboard panel wears. It exists so the
// panels stay about readings rather than chrome: one title row, one place
// for per-panel controls, one padding, and the detach action that honours
// the detachable-view contract (ADR-0108) by starting this executable again
// with `--panel <id>`.
Tk.Panel {
    id: panel

    property string panelId: ""
    // Controls that belong to this panel's header, right of the title.
    property alias controls: controlHost.data
    // A short reading shown in the header when the panel is the only thing
    // worth glancing at -- "98%", "31.2 GiB".
    property string summary: ""
    property color summaryColor: Tk.Theme.color.textMuted
    property bool detachable: true

    signal detachRequested(string panelId)

    padding: Tk.Theme.space.sm
    collapsible: false

    actions: Tk.Flex {
        align: Tk.Flex.Center
        gap: Tk.Theme.space.sm

        Tk.Mono {
            visible: panel.summary.length > 0
            text: panel.summary
            color: panel.summaryColor
        }
        Tk.Flex {
            id: controlHost
            align: Tk.Flex.Center
            gap: Tk.Theme.space.xs
            visible: children.length > 0
        }
        Tk.IconButton {
            visible: panel.detachable
            iconName: "external-link"
            small: true
            ghost: true
            tooltip: qsTr("Open this panel in its own window")
            onClicked: panel.detachRequested(panel.panelId)
        }
    }
}

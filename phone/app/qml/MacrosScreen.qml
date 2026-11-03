// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The desktop's macro library as a tap-to-run pad. Only ids and names arrive here; the steps stay on the desktop.
// AGENT-CONTRACT: link.macros is a list of {id, name} (macro.list), link.macroRunning the id being run ("" when idle),
// link.macroRunState running|done|cancelled|failed and link.macroReason a short code. Main.qml starts remote input while
// this page is shown, because macros are delivered through the same approved portal session.
ScreenFrame {
    id: root
    title: qsTr("Macros")
    subtitle: qsTr("Run your desktop macros")
    backVisible: true

    readonly property bool live: link.inputState === "active"
    readonly property bool busy: link.macroRunning !== ""

    InputStateNotice { }

    Tk.Notice {
        objectName: "macroHint"
        variant: "info"
        iconName: "info"
        text: qsTr("Edit macros in QindaQt Settings → Input → Macros on your desktop.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Tk.Notice {
        objectName: "macroResult"
        visible: !root.busy && (link.macroRunState === "failed" || link.macroRunState === "cancelled")
        variant: link.macroRunState === "failed" ? "danger" : "warning"
        title: link.macroRunState === "failed" ? qsTr("The macro did not finish") : qsTr("The macro was stopped")
        text: link.macroReason === "input-lost" ? qsTr("The desktop locked or remote input ended.")
            : link.macroReason === "input-unavailable" ? qsTr("This macro uses a window action that a phone cannot send.")
            : link.macroReason === "busy" ? qsTr("Another macro is already running on the desktop.")
            : link.macroReason === "denied" ? qsTr("Remote input is not approved yet.")
            : link.macroReason
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Tk.EmptyState {
        objectName: "noMacros"
        visible: link.macros.length === 0 && root.live
        iconName: "zap"
        title: qsTr("No macros yet")
        text: qsTr("Create macros in Settings → Input → Macros on your desktop and they appear here.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Repeater {
        model: link.macros
        delegate: Tk.Card {
            required property var modelData
            objectName: "macro_" + modelData.id
            // Not named `running`: Tk.Spinner has its own `running`, which would shadow this inside it.
            readonly property bool isRunning: link.macroRunning === modelData.id
            interactive: root.live && !root.busy
            tooltip: modelData.name
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: if (root.live && !root.busy) link.macroRun(modelData.id)
            Tk.Flex {
                direction: Tk.Flex.Row
                align: Tk.Flex.Center
                gap: Tk.Theme.space.md
                Tk.Icon { name: "zap"; size: Tk.Theme.size.iconLg }
                Tk.Label {
                    text: modelData.name
                    wrapMode: Text.Wrap
                    Tk.Flex.grow: 1
                    Tk.Flex.basis: 0
                }
                Tk.Spinner { visible: isRunning }
                Tk.Button {
                    objectName: "macroCancel"
                    visible: isRunning
                    text: qsTr("Stop")
                    variant: "danger"
                    small: true
                    onClicked: link.macroCancel()
                }
                Tk.Badge {
                    visible: !isRunning && link.macroRunState === "done" && link.macroLast === modelData.id
                    text: qsTr("Done")
                    variant: "success"
                }
            }
        }
    }
}

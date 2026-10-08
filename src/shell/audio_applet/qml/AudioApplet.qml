// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Bounded audio panel applet surface. The controller is the composed shell
// facade injected above QML; this file never imports the Audio1 client or
// any service module and owns no business policy of its own.
//
// AGENT-CONTRACT: the summary-icon rule below is pinned by
// tests/shell/audio_applet/verify_audio_summary_icon_contract.cmake, which
// greps THIS FILE for the exact expressions and icon names. It must stay
// here, spelled this way, even as the panel body moves elsewhere.
//
// The popup body lives in AudioAppletPanel.qml. Two reasons, both load
// bearing: this file would otherwise pass its decomposition limit as the
// panel gains bands, and the capture probe can render the panel directly in
// a plain window instead of chasing a popup's own QQuickPopupWindow.
Item {
    id: root

    objectName: "audioApplet"
    implicitWidth: 32
    implicitHeight: 28

    property var controller: null
    property bool vertical: false

    // The composed public SystemMenuController supplies the generic Settings
    // action. No service or app-launch authority is acquired inside QML.
    property var desktopControls: null

    readonly property bool showLists:
        controller?.phaseText === "ready"
            || controller?.phaseText === "degraded"

    // Band state lives on the applet, not on the Popup: the Popup's contents
    // are destroyed when it closes, and a section the user collapsed must
    // still be collapsed the next time they open the panel. It is deliberately
    // per-session — the desktop has no QML-side settings store, and adding a
    // Settings1 key is a behaviour change outside this lane.
    property bool outputExpanded: true
    property bool inputExpanded: true
    property bool appsExpanded: true
    property bool consoleExpanded: true
    // 0 means "ride whatever the service calls the default"; a non-zero serial
    // pins the band to one device for as long as it exists.
    property int outputSelectedSerial: 0
    property int inputSelectedSerial: 0

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Audio")
    Accessible.description: {
        if (!controller)
            return qsTr("Audio controls are not connected")
        if (controller.phaseText === "loading")
            return qsTr("Audio device information is loading")
        if (controller.phaseText === "unavailable")
            return qsTr("Audio is unavailable")
        return qsTr("Volume and mute controls for audio devices and application streams")
    }

    function defaultOutputRow() {
        if (!controller)
            return null
        const rows = controller.deviceRows ?? []
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].isOutput === true && rows[i].isDefault === true)
                return rows[i]
        }
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].isOutput === true)
                return rows[i]
        }
        return null
    }

    readonly property var outputRow: defaultOutputRow()
    readonly property string summaryIconName: {
        if (!outputRow || outputRow.muted)
            return "audio-volume-muted"
        const volume = Number(outputRow.volume ?? 0)
        if (volume <= 0) return "audio-volume-muted"
        if (volume < 1 / 3) return "audio-volume-low"
        if (volume < 2 / 3) return "audio-volume-medium"
        return "audio-volume-high"
    }

    T.ToolButton {
        id: summary
        objectName: "audioAppletSummary"
        anchors.fill: parent
        enabled: root.controller !== null
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.Accessible.name
        Accessible.description: root.Accessible.description

        function toggleDetails() {
            if (!enabled)
                return
            if (details.opened) details.close()
            else details.open()
        }
        onClicked: toggleDetails()
        Accessible.onPressAction: toggleDetails()
        Keys.onReturnPressed: event => {
            toggleDetails()
            event.accepted = true
        }
        Keys.onEnterPressed: event => {
            toggleDetails()
            event.accepted = true
        }

        contentItem: ShellIcons.Icon {
            objectName: "audioAppletIcon"
            anchors.centerIn: parent
            name: root.summaryIconName
            size: Math.min(20, root.height - Tokens.space["2"])
            color: Tokens.fg.default
            symbolic: true
            fallbackText: qsTr("Audio")
            Accessible.ignored: true
        }
        background: Item {}
    }

    C.PanelPopup {
        id: details

        // AGENT-CONTRACT: placement is owned by QindaQt.Controls.PanelPopup, the
        // one owner of panel-popup placement (docs/wiki/shell/panel-popup-placement.md):
        // it opens flush with the control's leading edge, above it on a bottom
        // panel, beside it on a side panel, and slides to stay on the output.
        // It also keeps the surface a real window, because a layer-shell panel
        // rejects keyboard focus and cannot paint outside its own band.
        anchorItem: summary
        vertical: root.vertical
        objectName: "audioAppletPopup"
        padding: Tokens.space["3"]
        // AGENT-GUARD: Screen dimensions are logical pixels. A dense mixer
        // must scroll inside the output, including a scaled/small output;
        // neither its content nor a physical-pixel export sizes the window.
        // Bind the selected item's screen facts and each-open revision, so
        // migration/removal recomputes without retaining a borrowed QScreen.
        readonly property size outputSpace: {
            const revision = placementRevision + (root.controller?.popupGeometryRevision ?? 0)
            const width = summary.Screen.width
            const height = summary.Screen.height
            if (revision < 0 || width <= 0 || height <= 0)
                return Qt.size(0, 0)
            return root.controller
                ? root.controller.popupAvailableSize(summary, summary.Screen.name)
                : Qt.size(0, 0)
        }
        readonly property real widthLimit: Math.max(1,
            outputSpace.width - padding * 2)
        readonly property real heightLimit: Math.max(1,
            outputSpace.height - summary.height - padding * 2)
        width: Math.min(360, widthLimit)
        height: Math.min(480, heightLimit,
            Math.max(160, panel.implicitHeight + padding * 2))
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
                     | T.Popup.CloseOnPressOutsideParent

        background: Rectangle {
            radius: Tokens.radius.l
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }

        contentItem: Flickable {
            id: scroller
            objectName: "audioAppletViewport"
            clip: true
            contentWidth: width
            contentHeight: panel.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                objectName: "audioAppletScrollbar"
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Audio controls scroll position")
            }

            function revealItem(item) {
                let cursor = item
                while (cursor && cursor !== panel)
                    cursor = cursor.parent
                if (!cursor || !item)
                    return
                const position = item.mapToItem(panel, 0, 0)
                const margin = Tokens.space["1"]
                if (position.y - margin < contentY)
                    contentY = Math.max(0, position.y - margin)
                else if (position.y + item.height + margin > contentY + height)
                    contentY = Math.min(Math.max(0, contentHeight - height),
                        position.y + item.height + margin - height)
            }

            function revealActiveFocus() {
                const window = panel.Window.window
                if (window)
                    revealItem(window.activeFocusItem)
            }

            // Preserve child slider/combo keys; page navigation bubbles here.
            Keys.onPressed: event => {
                const limit = Math.max(0, contentHeight - height)
                if (event.key === Qt.Key_PageDown)
                    contentY = Math.min(limit, contentY + height * 0.8)
                else if (event.key === Qt.Key_PageUp)
                    contentY = Math.max(0, contentY - height * 0.8)
                else if (event.key === Qt.Key_End && (event.modifiers & Qt.ControlModifier))
                    contentY = limit
                else if (event.key === Qt.Key_Home && (event.modifiers & Qt.ControlModifier))
                    contentY = 0
                else
                    return
                event.accepted = true
            }

            onHeightChanged: Qt.callLater(revealActiveFocus)
            onContentHeightChanged: Qt.callLater(revealActiveFocus)

            AudioAppletPanel {
                id: panel
                width: scroller.width - (scroller.contentHeight > scroller.height
                    ? Tokens.space["3"] : 0)
                height: implicitHeight
                controller: root.controller
                store: root
            }

            Connections {
                target: panel.Window.window
                enabled: target !== null
                function onActiveFocusItemChanged() { scroller.revealActiveFocus() }
            }
        }
    }
}

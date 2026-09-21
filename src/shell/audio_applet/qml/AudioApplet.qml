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

    // AGENT-NOTE: optional, and null in every composition today. The audio
    // applet has no way to open Settings — src/shell/qml/BuiltinAppletContent.qml
    // hands it only `controller` and `vertical`, while the
    // `desktopControlsAccess` object that carries openSettings() is passed to
    // other applets from the same file. That file is outside this lane, so the
    // seam is declared here and the footer action renders only once something
    // supplies it; nothing shows a button that cannot work.
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
        // A piece of desk equipment, not a menu: 420 px is what a device name,
        // a full-width fader, a readout and a mute need side by side without
        // the name wrapping onto three lines.
        width: 420
        height: Math.min(620, Math.max(160, panel.implicitHeight + padding * 2))
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
                     | T.Popup.CloseOnPressOutsideParent

        background: Rectangle {
            radius: Tokens.radius.l
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }

        contentItem: T.ScrollView {
            id: scroller
            clip: true
            // AGENT-GUARD: both dimensions are stated. A ScrollView left to
            // infer its content size from a Layout child reported a height
            // roughly one row short, so the popup sized itself just under its
            // content and clipped the last console strip with no scrollbar to
            // reach it. The popup height below reads the same number.
            contentWidth: availableWidth
            contentHeight: panel.implicitHeight

            AudioAppletPanel {
                id: panel
                width: scroller.availableWidth
                controller: root.controller
                store: root
            }
        }
    }
}

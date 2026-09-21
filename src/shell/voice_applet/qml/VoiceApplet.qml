// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Bounded voice panel applet surface. `access` is the composed shell facade
// injected above QML; this file never imports the Voice1 client or any service
// module and owns no policy of its own.
//
// AGENT-CONTRACT: every label, icon name and enablement decision comes from
// the controller's projection. This file must not compose a sentence from
// provider fields, and must not enable a control the projection disabled.
//
// The popup body lives in VoiceAppletPanel.qml for the same two reasons the
// audio applet splits: decomposition headroom, and so the capture probe can
// render the panel in a plain window instead of chasing a popup window.
Item {
    id: root

    objectName: "voiceApplet"
    implicitWidth: vertical ? 32 : Math.max(32, chip.implicitWidth + Tokens.space["2"])
    implicitHeight: 28

    property var access: null
    property var theme: null
    property bool vertical: false

    readonly property bool capturing: access?.capturing === true

    // While dictating, the chip shows the words as they are recognised when the
    // user allows it (services.voicePanelTranscript) and the state name when
    // they do not. The controller is what applies the preference: an empty
    // transcriptText here already means "not permitted or nothing heard yet".
    readonly property string chipText: {
        if (!access)
            return ""
        if (capturing && access.transcriptIsPartial === true)
            return access.transcriptText
        return access.summaryLabel
    }
    readonly property bool showSummaryText: !vertical && capturing

    Accessible.role: Accessible.Grouping
    Accessible.name: access ? access.accessibleName : qsTr("Voice")
    Accessible.description: access ? access.accessibleDescription
                                   : qsTr("Voice input is not connected")

    T.ToolButton {
        id: summary
        objectName: "voiceAppletSummary"
        anchors.fill: parent
        enabled: root.access !== null
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

        contentItem: Row {
            id: chip
            anchors.centerIn: parent
            spacing: Tokens.space["1"]

            ShellIcons.Icon {
                objectName: "voiceAppletIcon"
                anchors.verticalCenter: parent.verticalCenter
                name: root.access ? root.access.iconName
                                  : "audio-input-microphone-muted"
                size: Math.min(20, root.height - Tokens.space["2"])
                // Capture is the one state the chip must read across a room.
                color: root.capturing ? Tokens.accent.default
                     : root.access?.stateId === "error" ? Tokens.danger.default
                     : root.access?.voiceEnabled === true ? Tokens.fg.default
                     : Tokens.fg.disabled
                symbolic: true
                fallbackText: qsTr("Voice")
                Accessible.ignored: true

                // A steady pulse while the microphone is open. It stops the
                // moment capture does, so a stuck animation is itself a bug
                // report rather than ambient decoration.
                SequentialAnimation on opacity {
                    running: root.capturing
                    loops: Animation.Infinite
                    alwaysRunToEnd: true
                    NumberAnimation { to: 0.55; duration: 620; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.0; duration: 620; easing.type: Easing.InOutQuad }
                }
                onVisibleChanged: if (!visible) opacity = 1.0
            }

            VoiceLevelMeter {
                objectName: "voiceAppletMeter"
                anchors.verticalCenter: parent.verticalCenter
                visible: root.capturing
                active: root.capturing
                levelPercent: root.access ? root.access.levelPercent : 0
            }

            Text {
                objectName: "voiceAppletSummaryText"
                anchors.verticalCenter: parent.verticalCenter
                visible: root.showSummaryText
                text: root.chipText
                color: root.access?.transcriptIsPartial === true
                       ? Tokens.accent.default : Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pixelSize: Tokens.type.caption
                // A dictated sentence must never push the clock off the bar.
                // The words are a reassurance that capture is working; the
                // whole transcript is the popup's and the console's job.
                elide: Text.ElideLeft
                maximumLineCount: 1
                width: Math.min(implicitWidth, 180)
                Accessible.ignored: true
            }
        }
        background: Item {}
    }

    T.Popup {
        id: details
        objectName: "voiceAppletPopup"
        popupType: T.Popup.Window
        modal: false
        focus: true
        padding: Tokens.space["3"]
        // 380 px is what a status line, a transcript block and a row of
        // dictation actions need without the transcript wrapping to four lines.
        width: 380
        height: Math.min(560, Math.max(150, panel.implicitHeight + padding * 2))
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
                     | T.Popup.CloseOnPressOutsideParent

        onOpened: if (root.access) root.access.setExpanded(true)
        onClosed: if (root.access) root.access.setExpanded(false)

        background: Rectangle {
            radius: Tokens.radius.l
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }

        contentItem: T.ScrollView {
            id: scroller
            clip: true
            // AGENT-GUARD: both dimensions are stated. A ScrollView left to
            // infer its content size from a Layout child sizes the popup just
            // under its content and clips the last row with no scrollbar to
            // reach it. The popup height above reads the same number.
            contentWidth: availableWidth
            contentHeight: panel.implicitHeight

            VoiceAppletPanel {
                id: panel
                width: scroller.availableWidth
                access: root.access
            }
        }
    }
}

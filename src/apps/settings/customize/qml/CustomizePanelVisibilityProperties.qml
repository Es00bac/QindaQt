// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Panel visibility editor. Five dodge behaviors do not compress into icons a
// user can tell apart, so this one control is a selector: one line of state,
// a tooltip explaining the behavior, and an ordered list on open.
FormSurface {
    id: root

    required property var customizeSettings
    required property var properties

    readonly property var modes: [
        { token: "never", label: qsTr("Always visible"), tip: qsTr("The panel never moves out of the way") },
        { token: "intelligent", label: qsTr("Auto-hide"), tip: qsTr("The panel hides until the pointer reaches the screen edge") },
        { token: "dodge-active", label: qsTr("Dodge active window"), tip: qsTr("The panel moves aside for the active window") },
        { token: "dodge-all", label: qsTr("Dodge all windows"), tip: qsTr("The panel moves aside for any window") },
        { token: "maximized", label: qsTr("Hide when maximized"), tip: qsTr("The panel hides only while a window is maximized") }
    ]
    readonly property int currentModeIndex: {
        const token = root.properties.hideMode ?? "never"
        const found = root.modes.findIndex(mode => mode.token === token)
        return found >= 0 ? found : 0
    }

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["2"]

        Label {
            Layout.fillWidth: true
            text: qsTr("Visibility")
            muted: true
            font.pointSize: Tokens.type.caption
            Accessible.name: text
        }

        ComboBox {
            id: modeSelector

            objectName: "customizeVisibilitySelector"
            Layout.fillWidth: true
            model: root.modes.map(mode => mode.label)
            currentIndex: root.currentModeIndex
            enabled: root.customizeSettings.canEdit
            accessibleDescription: qsTr(
                "Choose when this panel stays out of the way")
            T.ToolTip.visible: modeHover.hovered
            T.ToolTip.delay: 500
            T.ToolTip.text: root.modes[root.currentModeIndex].tip
            onActivated: index => root.customizeSettings.configureSelectedPanel(
                              "hideMode", root.modes[index].token)

            HoverHandler { id: modeHover }
        }


        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Tokens.outline.divider
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Label {
                Layout.fillWidth: true
                text: qsTr("Hide delay · all panels")
                font.weight: Font.DemiBold
                Accessible.name: text
            }
            Label {
                text: root.customizeSettings.panelHideDelayAvailable
                      ? qsTr("%1 ms").arg(Math.round(delaySlider.value))
                      : qsTr("Unavailable")
                muted: true
                Accessible.name: text
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("How long an auto-hiding panel stays visible after the pointer leaves. Applies to every layout profile.")
            wrapMode: Text.Wrap
            muted: true
        }

        Slider {
            id: delaySlider

            objectName: "customizePanelHideDelaySlider"
            property bool draftActive: false

            function commitDraft() {
                if (!draftActive)
                    return
                const requested = Math.round(value)
                draftActive = false
                root.customizeSettings.setPanelHideDelayMs(requested)
                // AGENT-GUARD: Slider's local keyboard/pointer value is a
                // proposal. Rebind to Settings1 truth on refusal and while
                // awaiting a post-commit authoritative snapshot.
                Qt.callLater(function() {
                    delaySlider.value = Qt.binding(function() {
                        return root.customizeSettings.panelHideDelayMs
                    })
                })
            }

            Layout.fillWidth: true
            from: 0
            to: 5000
            stepSize: 50
            value: root.customizeSettings.panelHideDelayMs
            enabled: root.customizeSettings.panelHideDelayEditable
            accessibleName: qsTr("Auto-hide delay for all panels")
            accessibleDescription: qsTr("Milliseconds after the pointer leaves, from 0 to 5000. Applies to every layout profile.")
            onMoved: draftActive = true
            onPressedChanged: if (!pressed) commitDraft()
            Keys.onReleased: event => {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                        || event.key === Qt.Key_Home || event.key === Qt.Key_End)
                    commitDraft()
            }
        }

        Label {
            objectName: "customizePanelHideDelayStatus"
            Layout.fillWidth: true
            text: root.customizeSettings.panelHideDelayStatus
            wrapMode: Text.Wrap
            muted: !root.customizeSettings.panelHideDelayPending
            Accessible.name: text
        }
    }
}

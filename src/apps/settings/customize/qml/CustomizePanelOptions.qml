// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Panel behaviour that is not part of any layout: the global auto-hide delay
// (Settings1 panels.autoHideDelayMs, consumed by the shell's panel visibility
// runtime). It applies to every preset, so switching presets never resets it.
FormSurface {
    id: root

    required property var customizeSettings

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["2"]

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Label {
                Layout.fillWidth: true
                text: qsTr("Auto-hide delay · all panels")
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
            text: qsTr("How long an auto-hiding panel stays visible after the pointer leaves. Applies to every layout.")
            wrapMode: Text.Wrap
            muted: true
        }

        Slider {
            id: delaySlider

            objectName: "customizePanelHideDelaySlider"
            property bool draftActive: false
            property bool keyboardGestureActive: false

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
            accessibleDescription: qsTr("Milliseconds after the pointer leaves, from 0 to 5000. Applies to every layout.")
            onMoved: draftActive = true
            // QQuickSlider toggles pressed for keys too, including intermediate
            // auto-repeat releases. Only pointer release commits here.
            onPressedChanged: if (!pressed && !keyboardGestureActive) commitDraft()
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                        || event.key === Qt.Key_Home || event.key === Qt.Key_End)
                    keyboardGestureActive = true
            }
            Keys.onReleased: event => {
                // AGENT-GUARD: Auto-repeat emits intermediate releases while
                // the key is held. Writing then disables this pending slider
                // and loses later repeats; commit only the final release.
                if (!event.isAutoRepeat
                        && (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                            || event.key === Qt.Key_Home || event.key === Qt.Key_End)) {
                    keyboardGestureActive = false
                    commitDraft()
                }
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

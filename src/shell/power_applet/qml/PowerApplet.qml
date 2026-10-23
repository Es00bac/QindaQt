// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null
    readonly property var sessionActions: root.available ? root.access.sessionActions : null
    property string confirmationAction: ""
    property Item brightnessDragControl: null

    objectName: "powerApplet"
    readonly property string percentageText: available && /[0-9]+%/.test(access.batteryLabel) ? access.batteryLabel.match(/[0-9]+%/)[0] : ""
    readonly property string batteryIconName: {
        if (percentageText === "")
            return "battery-missing";
        const level = Number(percentageText.slice(0, -1));
        if (level >= 90)
            return "battery-100";
        if (level >= 70)
            return "battery-080";
        if (level >= 50)
            return "battery-060";
        if (level >= 30)
            return "battery-040";
        if (level >= 10)
            return "battery-020";
        return "battery-000";
    }

    implicitWidth: vertical ? 32 : (percentageText === "" ? 32 : 62)
    implicitHeight: 28

    ToolButton {
        id: summary
        objectName: "powerAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.access !== null ? root.access.accessibleName : qsTr("Power information is unavailable")
        Accessible.description: root.access !== null ? root.access.accessibleDescription : ""

        function openDetails() {
            if (root.available)
                details.open();
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: RowLayout {
            spacing: 4

            ShellIcons.Icon {
                objectName: "powerAppletIcon"
                name: root.batteryIconName
                size: Math.min(20, root.height - 8)
                color: Tokens.fg.default
                symbolic: true
                fallbackText: qsTr("Battery")
                Accessible.ignored: true
            }

            Text {
                objectName: "powerAppletPercentage"
                visible: !root.vertical && root.percentageText !== ""
                text: root.percentageText
                color: Tokens.fg.default
                font.pixelSize: 11
                textFormat: Text.PlainText
                Accessible.ignored: true
            }
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
        objectName: "powerAppletPopup"
        width: Math.min(340, Math.max(1, outputWidth() - 24))
        padding: 12
        height: Math.min(powerContents.implicitHeight + 64, 440, Math.max(1, outputHeight() - 48))

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"
        }

        contentItem: ColumnLayout {
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Label {
                    objectName: "powerAppletHeading"
                    Layout.fillWidth: true
                    text: qsTr("Power")
                    font.bold: true
                    color: Tokens.fg.default
                    Accessible.role: Accessible.Heading
                }
                C.Button {
                    objectName: "powerAppletSettingsButton"
                    text: qsTr("Settings…")
                    emphasized: false
                    visible: Boolean(root.access?.canOpenSettings)
                    accessibleDescription: qsTr("Open the Power page of Settings")
                    onClicked: {
                        if (root.access.openSettings())
                            details.close();
                    }
                }
            }
            Flickable {
                id: powerScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                objectName: "powerAppletScroll"
                implicitHeight: Math.min(powerContents.implicitHeight, 360)
                clip: true
                contentWidth: width
                contentHeight: powerContents.implicitHeight
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                // Keep scrolling separate from an active brightness gesture.
                // The passive pointer lifetime below survives popup forwarding;
                // transient changes to Slider.pressed must not unlock this view.
                interactive: root.brightnessDragControl === null
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                function revealFocusedControl() {
                    const focused = powerScroll.Window.window?.activeFocusItem;
                    let ancestor = focused;
                    while (ancestor && ancestor !== powerContents)
                        ancestor = ancestor.parent;
                    if (!ancestor || !focused)
                        return;
                    const y = focused.mapToItem(powerContents, 0, 0).y;
                    if (y < contentY)
                        contentY = Math.max(0, y);
                    else if (y + focused.height > contentY + height)
                        contentY = Math.min(Math.max(0, contentHeight - height),
                                            y + focused.height - height);
                }
                Connections {
                    target: powerScroll.Window.window
                    function onActiveFocusItemChanged() { powerScroll.revealFocusedControl(); }
                }
                ColumnLayout {
                    id: powerContents
                    width: powerScroll.width - (powerScroll.contentHeight > powerScroll.height ? 12 : 0)
                    spacing: 6

                    Label {
                        objectName: "powerAppletBatteryStatus"
                        Layout.fillWidth: true
                        text: root.access !== null ? root.access.accessibleName : qsTr("Power unavailable")
                        color: root.colors.text ?? "white"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.Heading
                    }

                    Label {
                        objectName: "powerAppletLoading"
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.phase === "loading"
                        text: qsTr("Power information is loading…")
                        color: root.colors.textMuted ?? "#a9afa9"
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.diagnostic !== ""
                        text: visible ? root.access.diagnostic : ""
                        color: root.colors.textMuted ?? "#a9afa9"
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.profileRows.length > 0
                        text: qsTr("Power profile")
                        color: root.colors.text ?? "white"
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: root.available ? (root.access.profileRows ?? []).length : 0

                            C.Button {
                                required property int index
                                readonly property var modelData: root.access?.profileRows?.[index] ?? ({
                                        "profileId": "",
                                        "label": "",
                                        "active": false,
                                        "adjustable": false,
                                        "pending": false,
                                        "accessibleName": "",
                                        "accessibleDescription": ""
                                    })

                                objectName: "powerAppletProfileButton"
                                Layout.fillWidth: true
                                emphasized: modelData.active
                                text: modelData.active ? qsTr("✓ %1").arg(modelData.label) : modelData.label
                                available: modelData.adjustable && !modelData.pending && !root.access.operationPending
                                focusPolicy: Qt.TabFocus
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: modelData.accessibleName
                                accessibleDescription: modelData.accessibleDescription
                                Accessible.checked: modelData.active
                                onClicked: root.access.requestProfile(modelData.profileId)
                                Accessible.onPressAction: {
                                    if (enabled)
                                        root.access.requestProfile(modelData.profileId);
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.keyboardRows.length > 0
                        text: qsTr("Keyboard brightness")
                        color: root.colors.text ?? "white"
                        font.bold: true
                    }

                    Repeater {
                        model: root.available ? (root.access.keyboardRows ?? []).length : 0

                        ColumnLayout {
                            required property int index
                            readonly property var modelData: root.access?.keyboardRows?.[index] ?? ({
                                    "controlId": "",
                                    "currentKnown": false,
                                    "normalizedCurrent": 0,
                                    "normalizedStep": 100,
                                    "adjustable": false,
                                    "pending": false,
                                    "accessibleName": "",
                                    "accessibleDescription": ""
                                })
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: parent.modelData.accessibleName
                                color: root.colors.text ?? "white"
                                elide: Text.ElideRight
                            }

                            C.Slider {
                                id: brightnessSlider
                                objectName: "powerAppletKeyboardSlider"
                                Layout.fillWidth: true
                                from: 0
                                to: 10000
                                stepSize: parent.modelData.normalizedStep
                                Binding {
                                    target: brightnessSlider
                                    property: "value"
                                    value: brightnessSlider.parent.modelData.currentKnown ? brightnessSlider.parent.modelData.normalizedCurrent : 0
                                    when: !brightnessSlider.gestureActive && !brightnessSlider.changedByUser
                                          && !brightnessSlider.parent.modelData.pending
                                    restoreMode: Binding.RestoreNone
                                }
                                enabled: parent.modelData.adjustable && !parent.modelData.pending && !root.access.operationPending
                                focusPolicy: Qt.TabFocus
                                Accessible.name: parent.modelData.accessibleName
                                Accessible.description: parent.modelData.accessibleDescription
                                property bool changedByUser: false
                                property string gestureControlId: ""
                                property int requestedValue: 0
                                readonly property bool gestureActive: brightnessPointer.active || pressed

                                // AGENT-GUARD: Popup.Window temporarily drops exclusive
                                // grabs on moves (Qt 6.11). Its Slider.pressed change is
                                // not a release; the passive point owns pointer lifetime.
                                PointHandler {
                                    id: brightnessPointer
                                    acceptedButtons: Qt.LeftButton
                                    target: null
                                    onActiveChanged: brightnessSlider.updateGesture()
                                }
                                function updateGesture() {
                                    if (brightnessPointer.active || brightnessSlider.pressed) {
                                        if (gestureControlId === "")
                                            gestureControlId = parent.modelData.controlId;
                                        root.brightnessDragControl = brightnessSlider;
                                    } else {
                                        // Release delivery updates the final position after
                                        // pointer handlers. Commit once after that event.
                                        Qt.callLater(commitGesture);
                                    }
                                }
                                function commitGesture() {
                                    if (brightnessPointer.active || brightnessSlider.pressed)
                                        return;
                                    if (root.brightnessDragControl === brightnessSlider)
                                        root.brightnessDragControl = null;
                                    if (changedByUser && enabled
                                            && gestureControlId === parent.modelData.controlId)
                                        root.access.requestKeyboardBrightness(gestureControlId, requestedValue);
                                    changedByUser = false;
                                    gestureControlId = "";
                                }
                                onMoved: {
                                    if (gestureControlId === "")
                                        gestureControlId = parent.modelData.controlId;
                                    changedByUser = true;
                                    requestedValue = Math.round(value);
                                    if (!brightnessPointer.active && !brightnessSlider.pressed)
                                        Qt.callLater(commitGesture);
                                }
                                onPressedChanged: updateGesture()

                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.access !== null && (root.access.peripheralRows ?? []).length > 0
                        text: qsTr("Device batteries")
                        color: root.colors.text ?? "white"
                        font.bold: true
                        Accessible.role: Accessible.Heading
                    }
                    Repeater {
                        model: root.available ? (root.access.peripheralRows ?? []).length : 0
                        ColumnLayout {
                            required property int index
                            readonly property var modelData: root.access?.peripheralRows?.[index] ?? ({
                                    "name": "",
                                    "detail": "",
                                    "accessibleName": ""
                                })
                            objectName: "powerAppletPeripheralRow"
                            Layout.fillWidth: true
                            spacing: 2
                            Accessible.role: Accessible.StaticText
                            Accessible.name: modelData.accessibleName
                            Label {
                                Layout.fillWidth: true
                                text: parent.modelData.name
                                textFormat: Text.PlainText
                                color: root.colors.text ?? "white"
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: parent.modelData.detail
                                textFormat: Text.PlainText
                                color: root.colors.textMuted ?? "#a9afa9"
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                    Label {
                        objectName: "powerAppletPeripheralDiagnostic"
                        Layout.fillWidth: true
                        text: root.access !== null ? (root.access.peripheralDiagnostic ?? "") : ""
                        visible: text !== ""
                        textFormat: Text.PlainText
                        color: root.colors.textMuted ?? "#a9afa9"
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Session")
                        color: root.colors.text ?? "white"
                        font.bold: true
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2

                        C.Button {
                            objectName: "powerAppletLockButton"
                            Layout.fillWidth: true
                            emphasized: false
                            text: qsTr("Lock")
                            available: root.sessionActions !== null && root.sessionActions.canLock && !root.sessionActions.pending
                            focusPolicy: Qt.TabFocus
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Lock session")
                            accessibleDescription: qsTr("Lock the current QindaQt session")
                            onClicked: root.sessionActions.requestLock()
                        }
                        C.Button {
                            objectName: "powerAppletLogoutButton"
                            Layout.fillWidth: true
                            emphasized: false
                            text: qsTr("Log out")
                            available: root.sessionActions !== null && root.sessionActions.canLogout && !root.sessionActions.pending
                            focusPolicy: Qt.TabFocus
                            Accessible.role: Accessible.Button
                            accessibleDescription: qsTr("Confirm and end the current QindaQt session")
                            onClicked: {
                                root.confirmationAction = "logout";
                                confirmation.open();
                            }
                        }
                        C.Button {
                            objectName: "powerAppletSuspendButton"
                            Layout.fillWidth: true
                            emphasized: false
                            text: qsTr("Suspend")
                            available: root.sessionActions !== null && root.sessionActions.canSuspend && !root.sessionActions.pending
                            focusPolicy: Qt.TabFocus
                            Accessible.role: Accessible.Button
                            accessibleDescription: qsTr("Suspend the computer now")
                            onClicked: root.sessionActions.requestSuspend()
                        }
                        C.Button {
                            objectName: "powerAppletRestartButton"
                            Layout.fillWidth: true
                            emphasized: false
                            text: qsTr("Restart")
                            available: root.sessionActions !== null && root.sessionActions.canReboot && !root.sessionActions.pending
                            focusPolicy: Qt.TabFocus
                            Accessible.role: Accessible.Button
                            accessibleDescription: qsTr("Confirm and restart the computer")
                            onClicked: {
                                root.confirmationAction = "reboot";
                                confirmation.open();
                            }
                        }
                        C.Button {
                            objectName: "powerAppletPowerOffButton"
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            emphasized: false
                            text: qsTr("Shut down")
                            available: root.sessionActions !== null && root.sessionActions.canPowerOff && !root.sessionActions.pending
                            focusPolicy: Qt.TabFocus
                            Accessible.role: Accessible.Button
                            accessibleDescription: qsTr("Confirm and shut down the computer")
                            onClicked: {
                                root.confirmationAction = "poweroff";
                                confirmation.open();
                            }
                        }
                    }

                    Label {
                        objectName: "powerAppletSessionFeedback"
                        Layout.fillWidth: true
                        visible: root.sessionActions !== null && root.sessionActions.feedback !== ""
                        text: visible ? root.sessionActions.feedback : ""
                        color: root.colors.warning ?? "#e5a84b"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    Label {
                        objectName: "powerAppletFeedback"
                        Layout.fillWidth: true
                        visible: root.access !== null && root.access.feedbackPresent
                        text: visible ? root.access.feedback : ""
                        color: root.colors.warning ?? "#e5a84b"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmation
        popupType: Popup.Window
        objectName: "powerAppletSessionConfirmation"
        width: 320
        modal: true
        focus: true
        title: root.confirmationAction === "logout" ? qsTr("Log out?") : root.confirmationAction === "reboot" ? qsTr("Restart?") : qsTr("Shut down?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            if (root.sessionActions === null)
                return;
            if (root.confirmationAction === "logout")
                root.sessionActions.requestLogout();
            else if (root.confirmationAction === "reboot")
                root.sessionActions.requestReboot();
            else if (root.confirmationAction === "poweroff")
                root.sessionActions.requestPowerOff();
        }
        contentItem: Label {
            text: root.confirmationAction === "logout" ? qsTr("Open applications will be asked to close before this session ends.") : root.confirmationAction === "reboot" ? qsTr("The computer will restart now.") : qsTr("The computer will shut down now.")
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Dialog
            Accessible.name: confirmation.title
            Accessible.description: text
        }
    }
}

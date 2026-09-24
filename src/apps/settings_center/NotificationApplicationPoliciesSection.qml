// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

Item {
    id: root

    required property var policies
    required property Item focusBefore
    required property Item focusAfter
    readonly property int applicationCount: policies ? policies.rowCount() : 0
    // AGENT-GUARD: ListView delegates exist only near the viewport. Keep
    // stable focus endpoints here; each proxy scrolls to its row before the
    // actual control receives focus, so ring edges never depend on itemAtIndex.
    readonly property Item firstControl: applicationCount > 0 ? firstFocusProxy : null
    readonly property Item lastControl: applicationCount > 0 ? lastFocusProxy : null

    implicitHeight: content.implicitHeight
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Per-application notifications")

    function focusControlAt(row, soundControl, retries) {
        if (row < 0 || row >= applicationCount)
            return
        applicationList.positionViewAtIndex(row, ListView.Contain)
        const item = applicationList.itemAtIndex(row)
        if (item) {
            (soundControl ? item.soundControl : item.muteControl).forceActiveFocus()
        } else if (retries > 0) {
            // Delegate creation follows layout. A bounded queued retry handles
            // that frame without leaving keyboard focus on the invisible proxy.
            Qt.callLater(() => root.focusControlAt(row, soundControl, retries - 1))
        }
    }

    function focusFirstControl() {
        focusControlAt(0, false, 4)
    }

    function focusLastControl() {
        focusControlAt(applicationCount - 1, true, 4)
    }

    function focusAfterIndex(row, fromSoundControl) {
        if (!fromSoundControl) {
            focusControlAt(row, true, 4)
        } else if (row + 1 < applicationCount) {
            focusControlAt(row + 1, false, 4)
        } else {
            root.focusAfter.forceActiveFocus()
        }
    }

    function focusBeforeIndex(row, soundControl) {
        if (soundControl) {
            focusControlAt(row, false, 4)
        } else if (row > 0) {
            focusControlAt(row - 1, true, 4)
        } else {
            root.focusBefore.forceActiveFocus()
        }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: Tokens.space["2"]

        Label {
            Layout.fillWidth: true
            text: qsTr("Per-application notifications")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.subtitle
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Mute blocks this app's popups. Active and Recent notifications remain available. Sound is an independent opt-in to the platform alert sound.")
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.StaticText
        }

        Label {
            objectName: "settingsNotificationPoliciesStatus"
            Layout.fillWidth: true
            visible: Boolean(root.policies?.statusText)
            text: root.policies?.statusText ?? ""
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: root.policies?.conflict || root.policies?.uncertain
                             || !root.policies?.available
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "settingsNotificationPoliciesError"
            Layout.fillWidth: true
            visible: Boolean(root.policies?.errorText)
            text: root.policies?.errorText ?? ""
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            Button {
                objectName: "settingsNotificationPoliciesRetry"
                visible: root.policies && (root.policies.conflict
                                           || root.policies.uncertain
                                           || !root.policies.available)
                enabled: root.policies && !root.policies.pending
                text: qsTr("Refresh")
                focusPolicy: Qt.StrongFocus
                KeyNavigation.tab: root.focusBefore
                KeyNavigation.backtab: root.lastControl ?? root.focusBefore
                onClicked: root.policies.retry()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.applicationCount === 0 && root.policies?.available
            text: qsTr("No installed applications were found.")
            textFormat: Text.PlainText
            Accessible.role: Accessible.StaticText
        }

        ListView {
            id: applicationList
            objectName: "settingsNotificationPoliciesList"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(250, Math.max(64, contentHeight))
            visible: root.applicationCount > 0
            clip: true
            model: root.policies
            spacing: Tokens.space["2"]
            T.ScrollBar.vertical: T.ScrollBar { policy: T.ScrollBar.AsNeeded }

            delegate: ColumnLayout {
                id: row
                required property int index
                required property string applicationId
                required property string displayName
                required property bool installed
                required property bool muted
                required property bool soundEnabled
                readonly property alias muteControl: muteSwitch
                readonly property alias soundControl: soundSwitch
                width: applicationList.width
                spacing: Tokens.space["1"]

                Label {
                    Layout.fillWidth: true
                    text: row.displayName
                    font.family: Tokens.type.fontFamily
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: row.installed ? row.applicationId
                                        : qsTr("Not installed · %1").arg(row.applicationId)
                    wrapMode: Text.WrapAnywhere
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]

                    Switch {
                        id: muteSwitch
                        objectName: "notificationMute-" + row.applicationId
                        text: qsTr("Mute")
                        checked: row.muted
                        enabled: root.policies?.canEdit ?? false
                        focusPolicy: Qt.StrongFocus
                        Accessible.role: Accessible.CheckBox
                        Accessible.name: qsTr("Mute notifications from %1").arg(row.displayName)
                        Accessible.description: qsTr("Suppresses this application's popups, including critical popups; Active and Recent remain.")
                        Accessible.checked: checked
                        Keys.priority: Keys.BeforeItem
                        Keys.onTabPressed: event => {
                            event.accepted = true
                            root.focusAfterIndex(row.index, false)
                        }
                        Keys.onBacktabPressed: event => {
                            event.accepted = true
                            root.focusBeforeIndex(row.index, false)
                        }
                        onClicked: {
                            root.policies.requestSetMuted(row.applicationId, !row.muted)
                            Qt.callLater(() => {
                                muteSwitch.checked = Qt.binding(() => row.muted)
                            })
                        }
                    }

                    CheckBox {
                        id: soundSwitch
                        objectName: "notificationSound-" + row.applicationId
                        text: qsTr("Sound")
                        checked: row.soundEnabled
                        enabled: root.policies?.canEdit ?? false
                        focusPolicy: Qt.StrongFocus
                        Accessible.role: Accessible.CheckBox
                        Accessible.name: qsTr("Play the platform alert sound for %1").arg(row.displayName)
                        Accessible.description: qsTr("Sound is opt-in and plays only when mute, Do Not Disturb, and lock privacy policies admit this notification.")
                        Accessible.checked: checked
                        Keys.priority: Keys.BeforeItem
                        Keys.onTabPressed: event => {
                            event.accepted = true
                            root.focusAfterIndex(row.index, true)
                        }
                        Keys.onBacktabPressed: event => {
                            event.accepted = true
                            root.focusBeforeIndex(row.index, true)
                        }
                        onClicked: {
                            root.policies.requestSetSoundEnabled(row.applicationId, !row.soundEnabled)
                            Qt.callLater(() => {
                                soundSwitch.checked = Qt.binding(() => row.soundEnabled)
                            })
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: Tokens.space["1"] / 2
                    color: Tokens.outline.divider
                    Accessible.ignored: true
                }
            }
        }
    }

    FocusScope {
        id: firstFocusProxy
        objectName: "notificationPoliciesFirstFocusProxy"
        width: 0
        height: 0
        visible: true
        opacity: 0
        Accessible.ignored: true
        onActiveFocusChanged: {
            if (activeFocus)
                root.focusFirstControl()
        }
    }

    FocusScope {
        id: lastFocusProxy
        objectName: "notificationPoliciesLastFocusProxy"
        width: 0
        height: 0
        visible: true
        opacity: 0
        Accessible.ignored: true
        onActiveFocusChanged: {
            if (activeFocus)
                root.focusLastControl()
        }
    }
}

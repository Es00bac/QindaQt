// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The Date & time route (ADR-0200). Every control here acts on a real
// service: the time zone and automatic time on org.freedesktop.timedate1, the
// first day of the week on the Settings1 key the Calendar's month grid reads.
// Nothing on this page is a knob with nothing behind it.
T.Page {
    id: root

    required property var dateTimeSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: timeZoneBox.enabled ? timeZoneBox
                                           : automaticTimeSwitch.enabled ? automaticTimeSwitch
                                           : root
    readonly property bool compact: width < 560

    title: qsTr("Date & time")
    background: Rectangle { color: Tokens.bg.base }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "dateTimePageHeading"
            Layout.fillWidth: true
            text: qsTr("Date & time")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Flickable {
            id: viewport
            objectName: "dateTimeFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Date and time settings scroll position")
            }

            FormSurface {
                id: formSurface
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["4"]

                    StateCard {
                        objectName: "dateTimeUnavailableNotice"
                        Layout.fillWidth: true
                        visible: !root.dateTimeSettings.available
                        status: StateCard.Error
                        title: qsTr("The system clock service is unavailable")
                        message: root.dateTimeSettings.statusText
                    }

                    StateCard {
                        objectName: "dateTimeErrorNotice"
                        Layout.fillWidth: true
                        visible: root.dateTimeSettings.errorText.length > 0
                        status: StateCard.Warning
                        title: qsTr("The change was not made")
                        message: root.dateTimeSettings.errorText
                    }

                    StateCard {
                        objectName: "dateTimeSynchronizationState"
                        Layout.fillWidth: true
                        visible: root.dateTimeSettings.available
                        status: root.dateTimeSettings.busy
                                ? StateCard.Busy
                                : root.dateTimeSettings.automaticTime
                                  && !root.dateTimeSettings.synchronizationText
                                        .includes(qsTr("Waiting"))
                                  ? StateCard.Success : StateCard.Information
                        title: qsTr("Clock")
                        message: root.dateTimeSettings.busy
                                 ? root.dateTimeSettings.statusText
                                 : root.dateTimeSettings.synchronizationText
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Time zone")
                        description: qsTr("Changing this affects the whole machine, so the system asks you to authenticate.")
                    }

                    FormRow {
                        objectName: "dateTimeTimeZoneRow"
                        Layout.fillWidth: true
                        label: qsTr("Time zone")
                        description: root.dateTimeSettings.timeZoneEditable
                            ? qsTr("Chosen from the list the system itself provides.")
                            : qsTr("The system did not offer a list of time zones, so this can only be changed with timedatectl.")
                        editor: timeZoneBox

                        ComboBox {
                            id: timeZoneBox
                            objectName: "dateTimeTimeZoneBox"
                            implicitWidth: root.compact ? 240 : 320
                            model: root.dateTimeSettings.timeZones
                            enabled: root.dateTimeSettings.timeZoneEditable
                            currentIndex: model.indexOf(root.dateTimeSettings.timeZone)
                            accessibleDescription: qsTr("Current time zone: %1")
                                .arg(root.dateTimeSettings.timeZone.length > 0
                                     ? root.dateTimeSettings.timeZone : qsTr("unknown"))
                            onActivated: root.dateTimeSettings.requestTimeZone(currentText)
                        }
                    }

                    Label {
                        objectName: "dateTimeCurrentZoneLabel"
                        Layout.fillWidth: true
                        visible: !root.dateTimeSettings.timeZoneEditable
                                 && root.dateTimeSettings.timeZone.length > 0
                        text: qsTr("Currently %1.").arg(root.dateTimeSettings.timeZone)
                        wrapMode: Text.WordWrap
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }

                    FormRow {
                        objectName: "dateTimeAutomaticRow"
                        Layout.fillWidth: true
                        label: qsTr("Set automatically")
                        description: root.dateTimeSettings.automaticTimeSupported
                            ? qsTr("Keeps the clock synchronized with a time server.")
                            : qsTr("This machine has no time-synchronization service, so the clock is set manually.")
                        editor: automaticTimeSwitch

                        Switch {
                            id: automaticTimeSwitch
                            objectName: "dateTimeAutomaticSwitch"
                            text: root.dateTimeSettings.automaticTime ? qsTr("On") : qsTr("Off")
                            checked: root.dateTimeSettings.automaticTime
                            enabled: root.dateTimeSettings.automaticTimeSupported
                            accessibleDescription: root.dateTimeSettings.synchronizationText
                            onToggled: root.dateTimeSettings.requestAutomaticTime(checked)
                        }
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Region")
                    }

                    FormRow {
                        objectName: "dateTimeWeekStartRow"
                        Layout.fillWidth: true
                        label: qsTr("First day of the week")
                        description: qsTr("Used by the Calendar's month grid. “Follow the region” takes it from the system locale.")
                        editor: weekStartBox

                        ComboBox {
                            id: weekStartBox
                            objectName: "dateTimeWeekStartBox"
                            implicitWidth: root.compact ? 200 : 260
                            model: [qsTr("Follow the region"), qsTr("Monday"), qsTr("Sunday")]
                            enabled: root.dateTimeSettings.weekStartEditable
                            currentIndex: Math.max(
                                0, root.dateTimeSettings.weekStarts.indexOf(
                                    root.dateTimeSettings.weekStart))
                            accessibleDescription: qsTr("The day the Calendar's month grid starts on.")
                            onActivated: root.dateTimeSettings.requestWeekStart(
                                root.dateTimeSettings.weekStarts[currentIndex])
                        }
                    }

                    FormRow {
                        objectName: "dateTimeLocaleRow"
                        Layout.fillWidth: true
                        label: qsTr("System locale")
                        // AGENT-NOTE: read-only on purpose. locale1 has no way
                        // to enumerate the locales a machine actually has
                        // generated, and a free-text field that can leave the
                        // session with an unusable locale is worse than a
                        // value you can read and change with localectl.
                        description: qsTr("Set for the whole machine with localectl. QindaQt shows it here so the region settings above make sense together.")
                        editor: localeField

                        TextField {
                            id: localeField
                            objectName: "dateTimeLocaleField"
                            implicitWidth: root.compact ? 200 : 260
                            readOnly: true
                            text: root.dateTimeSettings.locale.length > 0
                                  ? root.dateTimeSettings.locale : qsTr("unknown")
                            accessibleDescription: qsTr("The machine's locale, shown for reference.")
                        }
                    }

                    Label {
                        objectName: "dateTimeClockFormatNote"
                        Layout.fillWidth: true
                        text: qsTr("The panel clock's own format — 12 or 24 hour, seconds, the date — belongs to that clock. Change it where the clock lives, in Customize.")
                        wrapMode: Text.WordWrap
                        color: Tokens.fg.muted
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }
                }
            }
        }
    }
}

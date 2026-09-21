// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The Login screen route (ADR-0225): configures the SDDM greeter the whole
// machine sees before anyone logs in. Every control writes through the
// polkit-gated helper; nothing moves before the file on disk says it moved.
T.Page {
    id: root

    required property var loginScreenSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: sessionBox.enabled ? sessionBox : root
    readonly property bool compact: width < 560
    readonly property var sessionChoices: {
        const rows = [{ id: "", name: qsTr("Remember each user's last session") }]
        const source = root.loginScreenSettings.sessions ?? []
        for (let i = 0; i < source.length; ++i)
            rows.push(source[i])
        return rows
    }
    readonly property var numlockChoices: [
        { id: "none", name: qsTr("Do not change") },
        { id: "on", name: qsTr("Turn on") },
        { id: "off", name: qsTr("Turn off") }
    ]

    function indexOfChoice(choices, id) {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].id === id)
                return i
        }
        return 0
    }

    title: qsTr("Login screen")
    background: Rectangle { color: Tokens.bg.base }

    Keys.onPressed: event => {
        const pageStep = Math.max(1, viewport.height - Tokens.space["5"])
        if (event.key === Qt.Key_PageDown) {
            viewport.contentY = Math.min(
                        Math.max(0, viewport.contentHeight - viewport.height),
                        viewport.contentY + pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            viewport.contentY = Math.max(0, viewport.contentY - pageStep)
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "loginScreenPageHeading"
            Layout.fillWidth: true
            text: qsTr("Login screen")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("The screen the whole machine shows before anyone logs in (SDDM).")
            wrapMode: Text.Wrap
            muted: true
        }

        StateCard {
            objectName: "loginScreenReadOnlyNotice"
            Layout.fillWidth: true
            visible: !(root.loginScreenSettings.writable ?? false)
            status: StateCard.Information
            title: qsTr("You can view these settings but not change them")
            message: root.loginScreenSettings.readOnlyReason ?? ""
        }

        StateCard {
            objectName: "loginScreenErrorNotice"
            Layout.fillWidth: true
            visible: (root.loginScreenSettings.errorText ?? "").length > 0
            status: StateCard.Warning
            title: qsTr("The change was not made")
            message: root.loginScreenSettings.errorText ?? ""
        }

        StateCard {
            objectName: "loginScreenOverrideNotice"
            Layout.fillWidth: true
            visible: (root.loginScreenSettings.overrideText ?? "").length > 0
            status: StateCard.Warning
            title: qsTr("Another configuration file wins")
            message: root.loginScreenSettings.overrideText ?? ""
        }

        StateCard {
            objectName: "loginScreenBusyNotice"
            Layout.fillWidth: true
            visible: root.loginScreenSettings.busy ?? false
            status: StateCard.Busy
            title: qsTr("Saving")
            message: root.loginScreenSettings.statusText ?? ""
        }

        StateCard {
            objectName: "loginScreenStatusNotice"
            Layout.fillWidth: true
            visible: !(root.loginScreenSettings.busy ?? false)
                     && (root.loginScreenSettings.statusText ?? "").length > 0
            status: StateCard.Information
            title: qsTr("Configuration notice")
            message: root.loginScreenSettings.statusText ?? ""
        }

        Flickable {
            id: viewport
            objectName: "loginScreenFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: T.ScrollBar {}

            ColumnLayout {
                id: formColumn
                width: viewport.width
                spacing: Tokens.space["3"]

                FormSurface {
                    Layout.fillWidth: true
                    padding: Tokens.space["3"]
                    contentItem: ColumnLayout {
                        spacing: Tokens.space["2"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Login theme")
                            description: qsTr("The look of the login screen. QindaQt themes are listed first.")
                        }

                        Repeater {
                            id: themeRepeater
                            objectName: "loginScreenThemeRepeater"
                            model: (root.loginScreenSettings.themes ?? []).length

                            delegate: FormSurface {
                                id: themeRow
                                required property int index
                                readonly property var modelData:
                                    (root.loginScreenSettings.themes ?? [])[index] ?? null
                                visible: modelData !== null
                                Layout.fillWidth: true
                                padding: Tokens.space["2"]
                                Accessible.name: themeRow.modelData?.name ?? ""

                                contentItem: RowLayout {
                                    spacing: Tokens.space["3"]

                                    Rectangle {
                                        Layout.preferredWidth: 96
                                        Layout.preferredHeight: 54
                                        radius: Tokens.radius.s
                                        color: Tokens.bg.raised
                                        clip: true
                                        Accessible.ignored: true

                                        Image {
                                            anchors.fill: parent
                                            visible: (themeRow.modelData?.previewPath ?? "").length > 0
                                            source: visible
                                                ? "file://" + themeRow.modelData.previewPath
                                                : ""
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                        }

                                        Label {
                                            anchors.centerIn: parent
                                            visible: (themeRow.modelData?.previewPath ?? "").length === 0
                                            text: qsTr("No preview")
                                            muted: true
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Tokens.space["1"]

                                        Label {
                                            Layout.fillWidth: true
                                            text: themeRow.modelData?.name ?? ""
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: themeRow.modelData?.qindaqt ?? false
                                            text: qsTr("QindaQt theme")
                                            muted: true
                                        }
                                    }

                                    Button {
                                        objectName: "loginScreenThemeUse_"
                                                    + (themeRow.modelData?.id ?? "")
                                        readonly property bool isCurrent:
                                            themeRow.modelData?.current ?? false
                                        available: (root.loginScreenSettings.writable ?? false)
                                                   && !isCurrent
                                        emphasized: isCurrent
                                        text: isCurrent ? qsTr("Current")
                                                        : qsTr("Use this theme")
                                        accessibleDescription: isCurrent
                                            ? qsTr("%1 is the current login theme")
                                                .arg(themeRow.modelData?.name ?? "")
                                            : qsTr("Use %1 as the login theme")
                                                .arg(themeRow.modelData?.name ?? "")
                                        onClicked: themeRow.modelData !== null
                                            && root.loginScreenSettings.selectTheme(
                                                   themeRow.modelData.id)
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: themeRepeater.count === 0
                            text: qsTr("No login themes are installed.")
                            muted: true
                        }
                    }
                }

                FormSurface {
                    Layout.fillWidth: true
                    padding: Tokens.space["3"]
                    contentItem: ColumnLayout {
                        spacing: Tokens.space["2"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Session")
                            description: qsTr("SDDM has no machine-wide default session of its own: without automatic login it offers each user their last session. The session chosen here is the one automatic login starts.")
                        }

                        FormRow {
                            objectName: "loginScreenSessionRow"
                            Layout.fillWidth: true
                            label: qsTr("Default session")
                            editor: sessionBox

                            ComboBox {
                                id: sessionBox
                                objectName: "loginScreenSessionBox"
                                implicitWidth: root.compact ? 240 : 320
                                enabled: root.loginScreenSettings.writable ?? false
                                model: root.sessionChoices
                                textRole: "name"
                                currentIndex: root.indexOfChoice(
                                    root.sessionChoices,
                                    root.loginScreenSettings.defaultSession ?? "")
                                accessibleDescription: qsTr("Default session for automatic login")
                                onActivated: index => root.loginScreenSettings
                                    .selectDefaultSession(model[index].id)
                            }
                        }
                    }
                }

                FormSurface {
                    Layout.fillWidth: true
                    padding: Tokens.space["3"]
                    contentItem: ColumnLayout {
                        spacing: Tokens.space["2"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Automatic login")
                            description: qsTr("Starts the chosen user's session when the machine powers on.")
                        }

                        Label {
                            objectName: "loginScreenAutologinWarning"
                            Layout.fillWidth: true
                            text: qsTr("Anyone who can reach the machine gets this session — no password is asked. Turn this on only where that is acceptable.")
                            wrapMode: Text.Wrap
                        }

                        FormRow {
                            objectName: "loginScreenAutologinRow"
                            Layout.fillWidth: true
                            label: qsTr("Log in automatically")
                            editor: autologinSwitch

                            Switch {
                                id: autologinSwitch
                                objectName: "loginScreenAutologinSwitch"
                                enabled: root.loginScreenSettings.writable ?? false
                                checked: root.loginScreenSettings.autologinEnabled ?? false
                                text: qsTr("Log in automatically")
                                accessibleDescription: qsTr("Turn automatic login on or off")
                                onToggled: root.loginScreenSettings
                                    .setAutologinEnabled(checked)
                            }
                        }

                        FormRow {
                            objectName: "loginScreenAutologinUserRow"
                            Layout.fillWidth: true
                            label: qsTr("User")
                            description: qsTr("Automatic login starts the default session chosen above for this user.")
                            editor: autologinUserBox

                            ComboBox {
                                id: autologinUserBox
                                objectName: "loginScreenAutologinUserBox"
                                implicitWidth: root.compact ? 200 : 280
                                enabled: (root.loginScreenSettings.writable ?? false)
                                         && (root.loginScreenSettings.autologinEnabled ?? false)
                                model: root.loginScreenSettings.users ?? []
                                currentIndex: Math.max(0, model.indexOf(
                                    root.loginScreenSettings.autologinUser ?? ""))
                                accessibleDescription: qsTr("User logged in automatically")
                                onActivated: root.loginScreenSettings
                                    .selectAutologinUser(currentText)
                            }
                        }
                    }
                }

                FormSurface {
                    Layout.fillWidth: true
                    padding: Tokens.space["3"]
                    contentItem: ColumnLayout {
                        spacing: Tokens.space["2"]

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Behavior")
                            description: qsTr("Small behaviors SDDM applies before the session starts.")
                        }

                        FormRow {
                            objectName: "loginScreenNumlockRow"
                            Layout.fillWidth: true
                            label: qsTr("Numeric lock")
                            description: qsTr("Whether the numeric keypad is locked on when the login screen appears.")
                            editor: numlockBox

                            ComboBox {
                                id: numlockBox
                                objectName: "loginScreenNumlockBox"
                                implicitWidth: root.compact ? 200 : 280
                                enabled: root.loginScreenSettings.writable ?? false
                                model: root.numlockChoices
                                textRole: "name"
                                currentIndex: root.indexOfChoice(
                                    root.numlockChoices,
                                    root.loginScreenSettings.numlockMode ?? "none")
                                accessibleDescription: qsTr("Numeric lock state at the login screen")
                                onActivated: index => root.loginScreenSettings
                                    .setNumlockMode(model[index].id)
                            }
                        }

                        FormRow {
                            objectName: "loginScreenCursorThemeRow"
                            Layout.fillWidth: true
                            label: qsTr("Cursor theme")
                            description: qsTr("The pointer theme on the login screen. Empty keeps the system default.")
                            editor: cursorThemeField

                            TextField {
                                id: cursorThemeField
                                objectName: "loginScreenCursorThemeField"
                                implicitWidth: root.compact ? 200 : 280
                                enabled: root.loginScreenSettings.writable ?? false
                                text: root.loginScreenSettings.cursorTheme ?? ""
                                accessibleName: qsTr("Cursor theme")
                                onEditingFinished: root.loginScreenSettings
                                    .applyCursorTheme(text.trim())
                            }
                        }
                    }
                }
            }
        }
    }
}

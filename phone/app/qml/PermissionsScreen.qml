// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Permissions checklist. States come from link.perm (DeviceFacts.java); "later" rows are features
// this build does not implement yet and are shown as such, never with an invented state.
ScreenFrame {
    id: root
    title: qsTr("Permissions")
    subtitle: qsTr("What QindaLink may use on this phone")

    function stateOf(key) { return link.perm[key] === undefined ? "later" : link.perm[key] }

    Tk.Notice {
        variant: "info"
        title: qsTr("Set up from the desktop over USB")
        text: qsTr("Plug the phone in, open QindaLink on the desktop and choose Set up phone to grant these without tapping through Android settings.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    PermissionRow {
        permKey: "notificationAccess"; iconName: "bell"
        title: qsTr("Notification access")
        detail: qsTr("Show this phone's notifications on the desktop")
        state: root.stateOf(permKey)
        actionText: qsTr("Open notification access")
        onOpenRequested: key => link.openSettings(key)
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    PermissionRow {
        permKey: "sms"; iconName: "message-square"
        title: qsTr("SMS")
        detail: qsTr("Read and send text messages from the desktop")
        state: root.stateOf(permKey)
        actionText: qsTr("Open app settings")
        onOpenRequested: key => link.openSettings(key)
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    PermissionRow {
        permKey: "contacts"; iconName: "users"
        title: qsTr("Contacts")
        detail: qsTr("Show names instead of phone numbers")
        state: root.stateOf(permKey)
        actionText: qsTr("Open app settings")
        onOpenRequested: key => link.openSettings(key)
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    PermissionRow {
        permKey: "postNotifications"; iconName: "bell-ring"
        title: qsTr("Show QindaLink notifications")
        detail: qsTr("Keeps the connection notice visible while linked")
        state: root.stateOf(permKey)
        actionText: qsTr("Open app settings")
        onOpenRequested: key => link.openSettings(key)
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    PermissionRow {
        permKey: "batteryOptimisation"; iconName: "zap"
        title: qsTr("Unrestricted battery use")
        detail: qsTr("Stops Android putting the link to sleep")
        state: root.stateOf(permKey)
        actionText: qsTr("Open battery settings")
        onOpenRequested: key => link.openSettings(key)
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
}

// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The trailing status/action area of one Wi-Fi network row: "Saved" for a
// stored network, otherwise the password-prompt truth above Connect.
//
// AGENT-GUARD: the width is fixed (minimum = preferred = maximum) and equal
// in every row, so the signal meters before it line up (see the row layout in
// NetworkAccessPointSection.qml; tst_network_page asserts the alignment). The
// prompt wraps inside the area. Never size it from this row's own text.
ColumnLayout {
    id: area

    required property var accessPoint
    required property var networkSettings
    // Derived from the section width by the caller, so it is the same in
    // every row. The area is never narrower than Connect, whose text is also
    // the same in every row.
    required property real baseWidth
    readonly property real fixedWidth: Math.max(area.baseWidth,
                                                connectButton.implicitWidth)

    Layout.minimumWidth: area.fixedWidth
    Layout.preferredWidth: area.fixedWidth
    Layout.maximumWidth: area.fixedWidth
    Layout.alignment: Qt.AlignVCenter
    spacing: Tokens.space["1"]

    Label {
        Layout.fillWidth: true
        visible: area.accessPoint.saved
        horizontalAlignment: Text.AlignRight
        text: qsTr("Saved")
        muted: true
    }

    Label {
        objectName: "networkVisiblePrompt_" + area.accessPoint.id
        Layout.fillWidth: true
        visible: !area.accessPoint.saved
        horizontalAlignment: Text.AlignRight
        text: area.accessPoint.promptStatusText
        wrapMode: Text.Wrap
        muted: true
        Accessible.name: text
    }

    Button {
        id: connectButton
        objectName: "networkConnectVisible_" + area.accessPoint.id
        visible: !area.accessPoint.saved
        Layout.alignment: Qt.AlignRight
        available: area.accessPoint.connectAvailable
        busy: area.networkSettings.busy
        text: qsTr("Connect")
        accessibleDescription: area.accessPoint.promptStatusText
        onClicked: area.networkSettings.connectVisibleNetwork(area.accessPoint.id)
    }
}

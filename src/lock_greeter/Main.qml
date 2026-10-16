// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaTK as Tk
Rectangle {
    id: root
    objectName: "nativeLockRoot"
    color: "#08111e"
    focus: true
    Keys.onEscapePressed: authentication.cancel()
    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Locked session")
    property bool keyboardVisible: false
    readonly property bool saverReady: saverLoader.status === Loader.Ready
    property string clock: ""
    function updateClock() { clock = Qt.formatDateTime(new Date(), "hh:mm\ndddd, d MMMM") }
    // AGENT-CONTRACT (ADR-0382): the controller owns every decision about a
    // submission (send it, hold it for the first prompt, or keep it typed-ahead)
    // and clears the field itself through clearFields(). QML never inspects
    // prompt state, so Enter can never be dead.
    function submit() { authentication.submit(password.text) }
    Component.onCompleted: { updateClock(); password.forceActiveFocus() }
    Timer { interval: 1000; running: true; repeat: true; onTriggered: root.updateClock() }
    Loader { id: saverLoader; anchors.fill: parent; source: saver.scene; asynchronous: true }
    Rectangle {
        anchors.fill: parent
        color: "#66000000"
    }
    ColumnLayout {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.keyboardVisible ? -keyboardPanel.height / 2 : 0
        width: Math.min(parent.width - 32, 440)
        spacing: 18
        Tk.Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.clock
            horizontalAlignment: Text.AlignHCenter
            color: "white"
            font.pixelSize: 28
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: content.implicitHeight + 40
            color: Tk.Theme.color.panel
            radius: 12
            ColumnLayout {
                id: content
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12
                Tk.Label { text: authentication.user; font.bold: true; Layout.fillWidth: true }
                Tk.Label { text: authentication.promptText; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                // AGENT-GUARD: this field is never hidden or disabled while locked.
                // Gating it on a pending PAM prompt made keyboard-only unlock look
                // frozen whenever an attempt had ended (ADR-0382). A key press or
                // touch anywhere starts the PAM attempt; typing is never lost.
                Tk.TextField {
                    id: password
                    objectName: "nativeLockPassword"
                    Layout.fillWidth: true
                    focus: true
                    maximumLength: 1024
                    echoMode: authentication.secret ? TextInput.Password : TextInput.Normal
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    placeholderText: authentication.secret ? qsTr("Password") : qsTr("Response")
                    Accessible.name: authentication.secret ? qsTr("Session password") : qsTr("Authentication response")
                    onAccepted: root.submit()
                }
                Tk.Label { text: authentication.status; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                Tk.Label { text: authentication.layout; Layout.fillWidth: true; textFormat: Text.PlainText }
                RowLayout {
                    Layout.fillWidth: true
                    Tk.Button { text: qsTr("Keyboard"); onClicked: { root.keyboardVisible = !root.keyboardVisible; password.forceActiveFocus() } }
                    Item { Layout.fillWidth: true }
                    Tk.Button { text: qsTr("Cancel"); enabled: authentication.busy; onClicked: authentication.cancel() }
                    Tk.Button { objectName: "nativeLockUnlock"; text: qsTr("Unlock"); onClicked: root.submit() }
                }
            }
        }
    }
    Item {
        id: keyboardPanel
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 1000)
        height: keyboard.panelHeight
        visible: root.keyboardVisible
        function place() { keyboard.relayout(width, 48, 4, 8) }
        onWidthChanged: place()
        Component.onCompleted: place()
        Rectangle { anchors.fill: parent; color: Tk.Theme.color.panel }
        Repeater {
            model: keyboard.rows
            delegate: Item {
                required property int index
                required property var modelData
                property int rowIndex: index
                anchors.fill: parent
                Repeater {
                    model: modelData
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        x: modelData.x; y: modelData.y
                        width: modelData.width; height: modelData.height
                        color: tap.pressed ? Tk.Theme.color.bg : Tk.Theme.color.panel
                        border.width: 1; border.color: "#64748b"; radius: 6
                        Tk.Label { anchors.centerIn: parent; text: modelData.label }
                        TapHandler { id: tap; onTapped: { keyboard.press(rowIndex, index); password.forceActiveFocus() } }
                    }
                }
            }
        }
    }
    Connections {
        target: authentication
        function onClearFields() { password.clear() }
        // Return focus to the field after every state change (a button click
        // moves it), so the field is focused in every ready state.
        function onChanged() { password.forceActiveFocus() }
    }
    Connections {
        target: touchKeyboard
        function onInsertText(text) { password.insert(password.cursorPosition, text) }
        function onBackspace() { if (password.cursorPosition > 0) password.remove(password.cursorPosition - 1, password.cursorPosition) }
        function onSubmit() { root.submit() }
        function onHide() { root.keyboardVisible = false }
    }
}

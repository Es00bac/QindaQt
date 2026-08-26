// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Text {
    id: root

    required property var presentation
    required property var theme
    readonly property var colors: theme.colors ?? ({})
    readonly property bool busy: Boolean(presentation.operationBusy)
    readonly property string errorText:
        String(presentation.operationErrorText ?? "")
    readonly property bool hasError: errorText.length > 0
    readonly property string statusText:
        hasError ? errorText : (busy ? qsTr("Working…") : "")

    objectName: "notificationOperationStatus"
    visible: busy || hasError
    text: statusText
    color: hasError ? (colors.danger ?? "#f07c76")
                    : (colors.textMuted ?? "#a9afa9")
    font.pixelSize: 11
    elide: Text.ElideRight
    textFormat: Text.PlainText
    Accessible.role: hasError ? Accessible.AlertMessage : Accessible.StaticText
    Accessible.name: hasError
                     ? qsTr("Notification operation failed: %1").arg(text)
                     : (busy ? qsTr("Notification operation in progress") : "")
}

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
    }
}

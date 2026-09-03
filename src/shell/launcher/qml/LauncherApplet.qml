// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Compiled launcher applet: a summary button on the panel plus a non-modal
// browser popup. All state and policy live in the shell-private controller
// (access); this component owns presentation, keyboard traversal, and
// accessibility wiring only. The preview injects no controller and therefore
// shows a disabled, deterministic fallback.
Item {
    id: root

    required property var access
    property bool vertical: false
    readonly property bool available: access !== null

    objectName: "launcherApplet"
    implicitWidth: vertical ? 40 : Math.max(46, summary.implicitWidth + 12)
    implicitHeight: vertical ? 40 : 28

    // Flat cross-section traversal: section order is the focus order; each
    // section resolves its own delegates, so no stale item registry exists.
    readonly property int totalRows: {
        let count = 0
        const list = available ? access.sections : []
        for (let i = 0; i < list.length; ++i)
            count += list[i].items ? list[i].items.length : 0
        return count
    }

    function focusRow(flatIndex) {
        if (flatIndex < 0) {
            focusSearch()
            return
        }
        if (flatIndex >= totalRows)
            return
        for (let i = 0; i < sectionRepeater.count; ++i) {
            const sectionItem = sectionRepeater.itemAt(i)
            if (sectionItem && sectionItem.focusRowAt(flatIndex))
                return
        }
    }

    function focusSearch() {
        searchField.forceActiveFocus()
    }

    function openBrowser() {
        if (!root.available)
            return
        browser.open()
        searchField.forceActiveFocus()
    }

    T.ToolButton {
        id: summary

        objectName: "launcherAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: qsTr("Applications")
        Accessible.role: Accessible.Button
        Accessible.name: root.available
                         ? qsTr("Applications")
                         : qsTr("Application launcher is unavailable")
        Accessible.description: root.available
                                ? qsTr("Opens the application browser")
                                : ""

        onClicked: root.openBrowser()
        Accessible.onPressAction: root.openBrowser()

        contentItem: Text {
            text: summary.text
            color: summary.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
        }
        background: Item {}
    }

    T.Popup {
        id: browser

        objectName: "launcherAppletPopup"
        width: 340
        height: Math.min(480, contentColumn.implicitHeight + padding * 2)
        padding: Tokens.space["3"]
        modal: false
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Tokens.radius.l
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }

        contentItem: ColumnLayout {
            id: contentColumn

            spacing: Tokens.space["2"]

            TextField {
                id: searchField

                objectName: "launcherSearchField"
                Layout.fillWidth: true
                enabled: root.available
                text: root.available ? root.access.query : ""
                placeholderText: qsTr("Search applications")
                accessibleName: qsTr("Search applications")
                accessibleDescription:
                    qsTr("Type to filter applications; press Down to move to the results")
                onTextEdited: if (root.available)
                    root.access.query = text
                Keys.onDownPressed: root.focusRow(0)
                Keys.onReturnPressed: root.focusRow(0)
                Keys.onEnterPressed: root.focusRow(0)
            }

            Label {
                objectName: "launcherAppletDiagnostic"
                Layout.fillWidth: true
                visible: root.available && root.access.diagnostic !== ""
                text: visible ? root.access.diagnostic : ""
                muted: true
            }

            Label {
                objectName: "launcherAppletFeedback"
                Layout.fillWidth: true
                visible: root.available && root.access.feedback !== ""
                text: visible ? root.access.feedback : ""
                Accessible.role: Accessible.AlertMessage
            }

            Label {
                objectName: "launcherAppletEmpty"
                Layout.fillWidth: true
                visible: root.available && root.totalRows === 0
                         && root.access.phase !== "loading"
                text: root.access.query.length > 0
                      ? qsTr("No applications match the search")
                      : qsTr("No applications are installed")
                muted: true
            }

            T.ScrollView {
                objectName: "launcherAppletResults"
                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitHeight: Math.min(360, sectionColumn.implicitHeight)
                clip: true

                ColumnLayout {
                    id: sectionColumn

                    width: parent.width
                    spacing: Tokens.space["1"]

                    Repeater {
                        id: sectionRepeater

                        model: root.available ? root.access.sections : []

                        LauncherSection {
                            required property var modelData
                            required property int index

                            readonly property int computedBase: {
                                let base = 0
                                const list = root.access.sections
                                for (let i = 0; i < index; ++i)
                                    base += list[i].items ? list[i].items.length : 0
                                return base
                            }

                            Layout.fillWidth: true
                            section: modelData
                            controller: root.access
                            vertical: root.vertical
                            flatBase: computedBase
                            onFlatFocusRequested: flatIndex => root.focusRow(flatIndex)
                        }
                    }
                }
            }
        }
    }
}

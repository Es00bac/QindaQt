// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Shell.Icons 1.0 as ShellIcons
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
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    property bool dockHasLauncherGroup: false
    readonly property bool available: access !== null && Tokens.ready
    readonly property int summaryIconExtent:
        Math.max(0, Math.min(root.dockMode ? 40 : 20,
                             height - Tokens.space["2"]))

    objectName: "launcherApplet"
    implicitWidth: dockMode ? dockTileSize : 32
    implicitHeight: dockMode ? dockTileSize : 28

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
        if (flatIndex >= totalRows || !browserContent.item)
            return
        browserContent.item.focusRow(flatIndex)
    }

    function focusSearch() {
        if (browserContent.item)
            browserContent.item.focusSearch()
    }

    function openBrowser() {
        if (!root.available)
            return
        browser.open()
        focusSearch()
    }

    T.ToolButton {
        id: summary

        objectName: "launcherAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.available
                         ? qsTr("Applications")
                         : qsTr("Application launcher is unavailable")
        Accessible.description: root.available
                                ? qsTr("Opens the application browser")
                                : ""

        onClicked: root.openBrowser()
        Keys.onReturnPressed: root.openBrowser()
        Keys.onEnterPressed: root.openBrowser()
        Accessible.onPressAction: root.openBrowser()

        contentItem: ShellIcons.Icon {
            objectName: "launcherAppletIcon"
            anchors.centerIn: parent
            name: "start-here-kde"
            size: root.summaryIconExtent
            color: summary.enabled ? Tokens.fg.default : Tokens.fg.disabled
            fallbackText: qsTr("Applications")
            Accessible.ignored: true
        }
        background: Item {}
    }

    T.Popup {
        id: browser

        // AGENT-GUARD: panels reject keyboard focus and cannot paint outside
        // their surface. A separate popup window supplies both capabilities.
        popupType: T.Popup.Window
        objectName: "launcherAppletPopup"
        width: 340
        height: Math.min(480, (browserContent.item
                              ? browserContent.item.implicitHeight : 0)
                             + padding * 2)
        padding: Tokens.ready ? Tokens.space["3"] : 0
        modal: false
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Tokens.ready ? Tokens.radius.l : 0
            color: Tokens.ready ? Tokens.bg.raised : "transparent"
            border.color: Tokens.ready ? Tokens.outline.divider : "transparent"
        }

        contentItem: Loader {
            id: browserContent

            active: root.available
            sourceComponent: Component {
                ColumnLayout {
                    id: contentColumn

                    function focusRow(flatIndex) {
                        for (let i = 0; i < sectionRepeater.count; ++i) {
                            const sectionItem = sectionRepeater.itemAt(i)
                            if (sectionItem && sectionItem.focusRowAt(flatIndex))
                                return
                        }
                    }

                    function focusSearch() {
                        searchField.forceActiveFocus()
                    }

                    spacing: Tokens.space["2"]

                    Label {
                        objectName: "launcherAppletHeading"
                        Layout.fillWidth: true
                        text: qsTr("Applications")
                        Accessible.role: Accessible.Heading
                    }

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
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        Accessible.role: Accessible.AlertMessage
                    }

                    Label {
                        objectName: "launcherAppletPersistenceStatus"
                        Layout.fillWidth: true
                        visible: root.available
                                 && root.access.persistenceStatus !== ""
                        text: visible ? root.access.persistenceStatus : ""
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        Accessible.role: Accessible.AlertMessage
                    }

                    Label {
                        objectName: "launcherAppletFeedback"
                        Layout.fillWidth: true
                        visible: root.available && root.access.feedback !== ""
                        text: visible ? root.access.feedback : ""
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        Accessible.role: Accessible.AlertMessage
                    }

                    Label {
                        objectName: "launcherAppletEmpty"
                        Layout.fillWidth: true
                        visible: root.available && root.totalRows === 0
                                 && root.access.phase !== "loading"
                        text: root.available && root.access.query.length > 0
                              ? qsTr("No applications match the search")
                              : qsTr("No applications are installed")
                        muted: true
                    }

                    T.ScrollView {
                        id: resultsView
                        objectName: "launcherAppletResults"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        implicitHeight: Math.min(360, sectionColumn.implicitHeight)
                        contentWidth: availableWidth
                        clip: true
                        focusPolicy: Qt.NoFocus

                        ColumnLayout {
                            id: sectionColumn

                            width: resultsView.availableWidth
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
                                        for (let i = 0; i < index; ++i) {
                                            const earlierSection = list[i]
                                            if (earlierSection && earlierSection.items)
                                                base += earlierSection.items.length
                                        }
                                        return base
                                    }

                                    Layout.fillWidth: true
                                    section: modelData
                                    controller: root.access
                                    vertical: root.vertical
                                    flatBase: computedBase
                                    onFlatFocusRequested: flatIndex =>
                                        root.focusRow(flatIndex)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

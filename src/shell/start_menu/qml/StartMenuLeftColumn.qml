// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Left column of the start panel: the search field plus the launcher's
// sections rendered as one flat, keyboard-traversable program list on the
// Luna white body.
//
// AGENT-NOTE: the row data shape is the LauncherAppletController projection
// (sections[].identity, sections[].items[].{entryId, displayText, iconName,
// accessibleDescription, pinned}); identity → title translation mirrors
// LauncherSection.qml because the L0 model owns no user-facing strings.
// Activation re-enters controller.activate(entryId, "") exactly like the
// launcher rows, so grants and feedback stay behind one seam. The panel
// stays open after a launch, mirroring LauncherApplet's browser.
ColumnLayout {
    id: root

    required property var launcher
    readonly property bool ready: launcher !== null && Tokens.ready

    // Flat row count across all sections; section order is the focus order.
    readonly property int totalRows: {
        let count = 0
        const list = ready ? launcher.sections : []
        for (let i = 0; i < list.length; ++i)
            count += list[i].items ? list[i].items.length : 0
        return count
    }

    function focusSearch() {
        searchField.forceActiveFocus(Qt.PopupFocusReason)
    }

    function focusRow(flatIndex) {
        if (flatIndex < 0) {
            focusSearch()
            return
        }
        for (let i = 0; i < sectionRepeater.count; ++i) {
            const sectionItem = sectionRepeater.itemAt(i)
            if (sectionItem && sectionItem.focusRowAt(flatIndex))
                return
        }
    }

    // Translated from the stable identity the controller publishes; keep in
    // sync with LauncherSection.qml while the shapes match.
    function sectionTitle(identity) {
        switch (identity) {
        case "pinned": return qsTr("Pinned")
        case "recent": return qsTr("Recent")
        case "searchResults": return qsTr("Search results")
        case "utilities": return qsTr("Utilities")
        case "development": return qsTr("Development")
        case "education": return qsTr("Education")
        case "games": return qsTr("Games")
        case "graphics": return qsTr("Graphics")
        case "audioVideo": return qsTr("Audio & Video")
        case "network": return qsTr("Network")
        case "office": return qsTr("Office")
        case "science": return qsTr("Science")
        case "settings": return qsTr("Settings")
        case "system": return qsTr("System")
        default: return qsTr("Other")
        }
    }

    spacing: 0

    C.TextField {
        id: searchField

        objectName: "startMenuSearchField"
        Layout.fillWidth: true
        Layout.margins: 6
        enabled: root.ready
        text: root.ready ? root.launcher.query : ""
        placeholderText: qsTr("Search programs")
        accessibleName: qsTr("Search programs")
        accessibleDescription:
            qsTr("Type to filter programs; press Down to move to the results")
        onTextEdited: if (root.ready)
            root.launcher.query = text
        Keys.onDownPressed: root.focusRow(0)
        Keys.onReturnPressed: root.focusRow(0)
        Keys.onEnterPressed: root.focusRow(0)
    }

    C.Label {
        objectName: "startMenuProgramsUnavailable"
        Layout.fillWidth: true
        visible: !root.ready
        text: qsTr("The program list is unavailable")
        muted: true
    }

    C.Label {
        objectName: "startMenuEmpty"
        Layout.fillWidth: true
        visible: root.ready && root.totalRows === 0
        text: root.ready && root.launcher.query.length > 0
              ? qsTr("No programs match the search")
              : qsTr("No programs are installed")
        muted: true
    }

    T.ScrollView {
        id: resultsView

        objectName: "startMenuResults"
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        focusPolicy: Qt.NoFocus

        ColumnLayout {
            id: listColumn

            width: resultsView.availableWidth
            spacing: 2

            Repeater {
                id: sectionRepeater

                model: root.ready ? root.launcher.sections : []

                delegate: ColumnLayout {
                    id: sectionColumn

                    required property var modelData
                    required property int index

                    readonly property int flatBase: {
                        let base = 0
                        const list = root.launcher.sections
                        for (let i = 0; i < index; ++i) {
                            const earlier = list[i]
                            if (earlier && earlier.items)
                                base += earlier.items.length
                        }
                        return base
                    }
                    readonly property int rowCount: modelData.items
                                                    ? modelData.items.length : 0

                    function focusRowAt(flatIndex) {
                        if (flatIndex < flatBase
                                || flatIndex >= flatBase + rowCount)
                            return false
                        const row = rows.itemAt(flatIndex - flatBase)
                        if (!row)
                            return false
                        row.forceActiveFocus(Qt.PopupFocusReason)
                        return true
                    }

                    Layout.fillWidth: true
                    spacing: 0

                    C.Label {
                        objectName: "startMenuSectionHeader-"
                                    + (sectionColumn.modelData.identity ?? "")
                        Layout.fillWidth: true
                        visible: sectionColumn.rowCount > 0
                        text: root.sectionTitle(
                                  sectionColumn.modelData.identity ?? "")
                        muted: true
                        Accessible.role: Accessible.Heading
                    }

                    Repeater {
                        id: rows

                        model: sectionColumn.modelData.items ?? []

                        delegate: T.ItemDelegate {
                            id: row

                            required property var modelData
                            required property int index
                            readonly property int flatIndex:
                                sectionColumn.flatBase + index

                            objectName: "startMenuProgramRow-"
                                        + modelData.entryId
                            Layout.fillWidth: true
                            enabled: root.ready
                                     && root.launcher.launchGranted !== false
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus
                            implicitHeight: 32
                            leftPadding: 8
                            rightPadding: 8
                            Accessible.role: Accessible.ListItem
                            Accessible.name: modelData.displayText
                            Accessible.description:
                                String(modelData.accessibleDescription ?? "")

                            function launch() {
                                if (root.ready)
                                    root.launcher.activate(
                                        modelData.entryId, "")
                            }

                            onClicked: launch()
                            Keys.onReturnPressed: launch()
                            Keys.onEnterPressed: launch()
                            Keys.onSpacePressed: launch()
                            Keys.onUpPressed: root.focusRow(flatIndex - 1)
                            Keys.onDownPressed: root.focusRow(flatIndex + 1)
                            Accessible.onPressAction: launch()

                            // AGENT-GUARD: own both sides of the hover
                            // contrast pair — the white text may only ever
                            // appear on the XP selection blue, never on the
                            // light surface.
                            background: Rectangle {
                                objectName: "startMenuProgramRowBackground"
                                radius: 2
                                color: row.hovered || row.down
                                       ? "#2f6fd4" : "transparent"

                                C.FocusRing {
                                    anchors.fill: parent
                                    control: row
                                }
                            }

                            contentItem: Row {
                                spacing: 8

                                ShellIcons.Icon {
                                    anchors.verticalCenter:
                                        parent.verticalCenter
                                    name: String(row.modelData.iconName ?? "")
                                    size: 20
                                    fallbackText: row.modelData.displayText
                                    Accessible.ignored: true
                                }

                                C.Label {
                                    anchors.verticalCenter:
                                        parent.verticalCenter
                                    width: parent.width - parent.spacing - 20
                                    text: row.modelData.displayText
                                    color: row.hovered || row.down
                                           ? "#ffffff" : "#1f1d17"
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
        implicitHeight: Math.min(380, listColumn.implicitHeight)
    }
}

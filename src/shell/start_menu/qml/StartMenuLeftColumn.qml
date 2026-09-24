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
//
// AGENT-GUARD: this list must instantiate only the rows it can show. The
// projection is flattened into one array whose index IS the flat program
// index, and a ListView with native sections renders it, so opening the panel
// builds a viewport of delegates instead of the whole catalogue. A nested
// Repeater built every row — 336 of 336 on a normal install, each resolving
// an icon through the theme — inside popup.open(), and that is what made the
// start menu sluggish (qindaqt.start-menu-qml pins the budget).
ColumnLayout {
    id: root

    required property var launcher
    readonly property bool ready: launcher !== null && Tokens.ready

    // AGENT-GUARD: index i of this array is flat program index i, which is
    // also the ListView's currentIndex and the focus order. Section headers
    // are drawn by ListView.section, never as model rows, so nothing has to
    // translate between two indexing schemes.
    readonly property var programs: root.flatten(ready ? launcher.sections : [])
    readonly property int totalRows: programs.length
    // Distinct contiguous runs, for the height arithmetic below.
    readonly property int sectionCount: {
        let count = 0
        let previous = ""
        for (let i = 0; i < programs.length; ++i) {
            const identity = programs[i].sectionIdentity
            if (identity !== previous) {
                ++count
                previous = identity
            }
        }
        return count
    }
    // Exact extent from the model rather than from a laid-out content item: a
    // lazy view knows its own height only for the rows it has created, and
    // the panel's height binding must not depend on scrolling.
    readonly property int listExtent: totalRows * rowExtent
                                      + sectionCount * sectionExtent

    readonly property int rowExtent: 32
    readonly property int sectionExtent: 20

    function flatten(sections) {
        const rows = []
        for (let i = 0; i < sections.length; ++i) {
            const section = sections[i]
            const items = section && section.items ? section.items : []
            const identity = String(section && section.identity
                                    ? section.identity : "")
            for (let j = 0; j < items.length; ++j) {
                const item = items[j]
                rows.push({
                    "sectionIdentity": identity,
                    "entryId": String(item.entryId ?? ""),
                    "displayText": String(item.displayText ?? ""),
                    "iconName": String(item.iconName ?? ""),
                    "accessibleDescription":
                        String(item.accessibleDescription ?? ""),
                    "pinned": Boolean(item.pinned)
                })
            }
        }
        return rows
    }

    // Pin to Dock / Remove from Dock (ADR-0265) through the launcher's own
    // pin mutation; one menu for the column, retargeted on open.
    function openProgramMenu(program, anchor) {
        programMenu.program = program
        programMenu.popup(anchor)
    }

    function focusSearch() {
        searchField.forceActiveFocus(Qt.PopupFocusReason)
    }

    // AGENT-GUARD: a lazy view has no item for an off-screen index, so the row
    // is brought into the viewport first and focused on the item the view then
    // owns. Focusing a stale currentItem strands Down at the viewport edge.
    function focusRow(flatIndex) {
        if (flatIndex < 0) {
            focusSearch()
            return
        }
        if (flatIndex >= root.totalRows)
            return
        resultsView.currentIndex = flatIndex
        resultsView.positionViewAtIndex(flatIndex, ListView.Contain)
        resultsView.forceLayout()
        if (resultsView.currentItem !== null)
            resultsView.currentItem.forceActiveFocus(Qt.PopupFocusReason)
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

    T.Menu {
        id: programMenu
        objectName: "startMenuProgramMenu"
        popupType: T.Popup.Window

        property var program: ({})

        T.MenuItem {
            objectName: "startMenuTogglePin"
            text: Boolean(programMenu.program.pinned) ? qsTr("Remove from Dock")
                                                      : qsTr("Pin to Dock")
            onTriggered: {
                if (!root.ready)
                    return
                const entryId = String(programMenu.program.entryId ?? "")
                if (Boolean(programMenu.program.pinned))
                    root.launcher.unpin(entryId)
                else
                    root.launcher.pin(entryId)
            }
        }
    }

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

    ListView {
        id: resultsView

        objectName: "startMenuResults"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: Math.min(380, root.listExtent)
        visible: root.totalRows > 0
        clip: true
        spacing: 2
        // A viewport plus a little travel, never the catalogue. Delegates are
        // recycled so scrolling does not keep allocating rows either.
        cacheBuffer: root.rowExtent * 6
        reuseItems: true
        // Focus lives on the delegate the panel moved to, so the view itself
        // must not also consume Up/Down or steal focus from that row.
        keyNavigationEnabled: false
        focus: false
        boundsBehavior: Flickable.StopAtBounds
        model: root.programs

        T.ScrollBar.vertical: T.ScrollBar {
            objectName: "startMenuResultsScrollBar"
            policy: resultsView.contentHeight > resultsView.height
                    ? T.ScrollBar.AlwaysOn : T.ScrollBar.AlwaysOff
        }

        section.property: "sectionIdentity"
        section.criteria: ViewSection.FullString
        section.delegate: C.Label {
            required property string section

            objectName: "startMenuSectionHeader-" + section
            width: resultsView.width
            height: root.sectionExtent
            text: root.sectionTitle(section)
            muted: true
            Accessible.role: Accessible.Heading
        }

        delegate: T.ItemDelegate {
            id: row

            required property var modelData
            required property int index

            objectName: "startMenuProgramRow-" + modelData.entryId
            width: resultsView.width
            height: root.rowExtent
            enabled: root.ready && root.launcher.launchGranted !== false
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            leftPadding: 8
            rightPadding: 8
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData.displayText
            Accessible.description: modelData.accessibleDescription

            function launch() {
                if (root.ready)
                    root.launcher.activate(row.modelData.entryId, "")
            }

            onClicked: launch()
            Keys.onReturnPressed: launch()
            Keys.onEnterPressed: launch()
            Keys.onSpacePressed: launch()
            Keys.onUpPressed: root.focusRow(row.index - 1)
            Keys.onDownPressed: root.focusRow(row.index + 1)
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Menu
                        || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                    root.openProgramMenu(row.modelData, row)
                    event.accepted = true
                }
            }
            Accessible.onPressAction: launch()

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: root.openProgramMenu(row.modelData, row)
            }

            // AGENT-GUARD: own both sides of the hover contrast pair — the
            // white text may only ever appear on the XP selection blue, never
            // on the light surface.
            background: Rectangle {
                objectName: "startMenuProgramRowBackground"
                radius: 2
                color: row.hovered || row.down ? "#2f6fd4" : "transparent"

                C.FocusRing {
                    anchors.fill: parent
                    control: row
                }
            }

            contentItem: Row {
                spacing: 8

                ShellIcons.Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: row.modelData.iconName
                    size: 20
                    fallbackText: row.modelData.displayText
                    Accessible.ignored: true
                }

                C.Label {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - parent.spacing - 20
                    text: row.modelData.displayText
                    color: row.hovered || row.down ? "#ffffff" : "#1f1d17"
                    elide: Text.ElideRight
                }
            }
        }
    }
}

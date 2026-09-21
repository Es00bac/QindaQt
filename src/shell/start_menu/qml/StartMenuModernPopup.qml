// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// The modern Windows start panel (ADR-0224), selected by
// `settings.variant: "modern"` on a start-menu instance. It is the second
// Windows start menu QindaQt reproduces: a centred rounded card with a search
// field, a grid of pinned applications, the complete program list behind an
// "All apps" toggle, and a session footer.
//
// AGENT-CONTRACT: this is a sibling of StartMenuPopup, not a replacement.
// StartMenuApplet picks one by variant and the Luna panel keeps its exact
// dressing; Bliss is the reference layout and must not change when this file
// does. Both consume the same two borrowed facades and the same
// launcher.activate(entryId, "") seam, so grants and feedback stay behind one
// boundary.
//
// AGENT-NOTE: unlike the Luna panel this surface is tokenized. Windows 11's
// start menu has no fixed period palette to reproduce — it follows the system
// accent and light/dark mode — so following QST-1 is the faithful choice as
// well as the maintainable one.
//
// AGENT-CONTRACT: placement is owned by QindaQt.Controls.PanelPopup
// (docs/wiki/shell/panel-popup-placement.md): a centred taskbar button opens
// this panel directly above itself, and the panel slides along the panel axis
// to stay on the output.
C.PanelPopup {
    id: popup

    required property var launcher
    required property var controls

    readonly property bool launcherReady: launcher !== null && Tokens.ready
    readonly property bool controlsReady: controls !== null && Tokens.ready
    readonly property var sessionMenu: controlsReady
        ? controls.systemMenu ?? null : null
    readonly property var session: sessionMenu !== null
        && sessionMenu.sessionActionsAvailable ? sessionMenu.sessionActions
                                               : null

    // Revealed by the "All apps" toggle. Pinned-only is the resting state, so
    // the panel opens at the size Windows 11 opens at.
    property bool showingAllApps: false

    // The launcher projection's pinned section, flattened to its items. An
    // absent section is an empty grid, never an error.
    readonly property var pinned: {
        if (!popup.launcherReady)
            return []
        const sections = popup.launcher.sections
        for (let i = 0; i < sections.length; ++i) {
            const section = sections[i]
            if (section && String(section.identity ?? "") === "pinned")
                return section.items ?? []
        }
        return []
    }

    function sessionEnabled(capability) {
        return popup.session !== null && Boolean(popup.session[capability])
               && !popup.session.pending
    }

    // The confirmation UX is owned by StartMenuApplet, exactly as for the
    // Luna panel: a dialog nested inside a popup closes with its parent.
    signal logOffRequested()

    objectName: "startMenuModernPopup"
    width: 520
    height: Math.min(640, panelColumn.implicitHeight + padding * 2)
    padding: Tokens.space["3"]

    onOpened: Qt.callLater(function() {
        if (popup.launcherReady)
            searchField.forceActiveFocus(Qt.PopupFocusReason)
    })
    onClosed: popup.showingAllApps = false

    background: Rectangle {
        objectName: "startMenuModernChrome"
        radius: Tokens.radius.l
        color: Tokens.bg.raised
        border.width: 1
        border.color: Tokens.outline.divider
    }

    contentItem: ColumnLayout {
        id: panelColumn

        spacing: Tokens.space["3"]

        C.TextField {
            id: searchField

            objectName: "startMenuModernSearchField"
            Layout.fillWidth: true
            enabled: popup.launcherReady
            text: popup.launcherReady ? popup.launcher.query : ""
            placeholderText: qsTr("Search for apps and files")
            accessibleName: qsTr("Search for apps and files")
            accessibleDescription:
                qsTr("Type to filter applications; results replace the pinned grid")
            onTextEdited: if (popup.launcherReady)
                popup.launcher.query = text
            // Typing is itself the "all apps" gesture: a query has to search
            // everything, not only what happens to be pinned.
            Keys.onDownPressed: allApps.focusRow(0)
        }

        C.Label {
            objectName: "startMenuModernUnavailable"
            Layout.fillWidth: true
            visible: !popup.launcherReady
            text: qsTr("The program list is unavailable")
            muted: true
        }

        RowLayout {
            Layout.fillWidth: true
            visible: popup.launcherReady && !popup.searching

            C.Label {
                objectName: "startMenuModernPinnedHeading"
                Layout.fillWidth: true
                text: qsTr("Pinned")
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
            }

            C.Button {
                objectName: "startMenuModernAllAppsButton"
                text: popup.showingAllApps ? qsTr("Pinned") : qsTr("All apps")
                accessibleDescription: popup.showingAllApps
                    ? qsTr("Show the pinned grid again")
                    : qsTr("Show every installed application")
                onClicked: popup.showingAllApps = !popup.showingAllApps
            }
        }

        // The pinned grid: five columns of icon-over-label tiles, the
        // arrangement Windows 11 uses. Hidden while searching or while the
        // complete list is showing, so only one program surface is ever live.
        GridView {
            id: pinnedGrid

            objectName: "startMenuModernPinnedGrid"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(260, pinnedGrid.contentHeight)
            visible: popup.launcherReady && !popup.showingAllApps
                     && !popup.searching && popup.pinned.length > 0
            clip: true
            cellWidth: Math.floor(width / 5)
            cellHeight: 92
            // A pinned grid is short, but recycle anyway: the same rule as the
            // Luna program list (never build rows the panel cannot show).
            reuseItems: true
            cacheBuffer: cellHeight * 2
            model: popup.pinned

            delegate: T.ItemDelegate {
                id: tile

                required property var modelData
                required property int index

                objectName: "startMenuModernPinnedTile-" + modelData.entryId
                width: pinnedGrid.cellWidth
                height: pinnedGrid.cellHeight
                enabled: popup.launcherReady
                         && popup.launcher.launchGranted !== false
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.role: Accessible.ListItem
                Accessible.name: String(tile.modelData.displayText ?? "")
                Accessible.description:
                    String(tile.modelData.accessibleDescription ?? "")

                function launch() {
                    if (popup.launcherReady)
                        popup.launcher.activate(tile.modelData.entryId, "")
                }

                onClicked: launch()
                Keys.onReturnPressed: launch()
                Keys.onEnterPressed: launch()
                Keys.onSpacePressed: launch()
                Accessible.onPressAction: launch()

                background: Rectangle {
                    radius: Tokens.radius.m
                    color: tile.hovered || tile.down ? Tokens.bg.highest
                                                     : "transparent"

                    C.FocusRing {
                        anchors.fill: parent
                        control: tile
                    }
                }

                contentItem: ColumnLayout {
                    spacing: Tokens.space["1"]

                    ShellIcons.Icon {
                        Layout.alignment: Qt.AlignHCenter
                        name: String(tile.modelData.iconName ?? "")
                        size: 40
                        fallbackText: String(tile.modelData.displayText ?? "")
                        Accessible.ignored: true
                    }

                    C.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: String(tile.modelData.displayText ?? "")
                        font.pointSize: Tokens.type.caption
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        C.Label {
            objectName: "startMenuModernNoPins"
            Layout.fillWidth: true
            visible: popup.launcherReady && !popup.showingAllApps
                     && !popup.searching && popup.pinned.length === 0
            text: qsTr("Nothing is pinned yet")
            muted: true
        }

        // The complete program list, reusing the same searchable, lazily
        // rendered column the Luna panel uses so there is one program list
        // implementation and one keyboard contract.
        StartMenuLeftColumn {
            id: allApps

            objectName: "startMenuModernAllApps"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: popup.launcherReady
                     && (popup.showingAllApps || popup.searching)
            launcher: popup.launcher
        }

        Rectangle {
            objectName: "startMenuModernFooter"
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Tokens.outline.divider
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            C.Label {
                objectName: "startMenuModernUser"
                Layout.fillWidth: true
                text: qsTr("This session")
                muted: true
            }

            C.Button {
                id: logOffButton

                objectName: "startMenuModernLogOffButton"
                enabled: popup.sessionEnabled("canLogout")
                text: qsTr("Log Off")
                accessibleDescription:
                    qsTr("Confirm and end the current session")
                onClicked: popup.logOffRequested()
            }
        }
    }

    // A non-empty query means the panel is searching, so the pinned grid gives
    // way to the result list. Declared after the content so the bindings above
    // can read it.
    readonly property bool searching: launcherReady
        && String(launcher.query ?? "").length > 0
}

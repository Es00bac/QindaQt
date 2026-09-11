// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import QindaQt.Shell.Icons 1.0 as ShellIcons

// XFCE-style Applications popup: the launcher's sections rendered as a
// scrollable window popup, opened from the desktop with the configured
// modifier held (or from the traditional style's "Applications" entry). Rows
// dispatch through the same launcher facade the Start menu uses; a null
// facade yields an empty popup, never a crash.
//
// AGENT-GUARD: popupType Window (panel precedent) — the desktop surface is a
// focus-less layer-shell toplevel and cannot host focused Item popups.
T.Popup {
    id: root

    // Borrowed LauncherAppletController facade; may be null.
    property var launcherAccess: null

    popupType: T.Popup.Window
    objectName: "desktopApplicationsMenu"
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    width: 320
    height: Math.min(480, list.contentHeight + topPadding + bottomPadding)
    padding: 4
    x: 8
    y: parent !== null ? Math.max(8, parent.height - height - 8) : 8

    background: Rectangle {
        color: "#ee20242a"
        radius: 6
        border.color: "#3c433f"
    }

    contentItem: ListView {
        id: list
        objectName: "desktopApplicationsList"

        model: root.launcherAccess !== null ? root.launcherAccess.sections : []
        clip: true
        spacing: 2
        reuseItems: false
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollIndicator.vertical: T.ScrollIndicator { }

        delegate: Column {
            id: sectionColumn
            objectName: "desktopApplicationsSection"

            required property var modelData

            width: ListView.view.width
            topPadding: 4
            spacing: 2

            T.Label {
                objectName: "desktopApplicationsSectionTitle"
                width: parent.width
                leftPadding: 8
                text: root.sectionTitle(sectionColumn.modelData.identity)
                color: "#9feeeeee"
                font.pixelSize: 11
                font.capitalization: Font.SmallCaps
            }

            Repeater {
                model: sectionColumn.modelData.items ?? []

                delegate: T.ItemDelegate {
                    id: appRow
                    objectName: "desktopApplicationsRow"

                    required property var modelData

                    // The delegate binds before the Repeater parents it;
                    // guard the null parent or every row logs a TypeError.
                    width: parent ? parent.width : 0
                    height: 32
                    text: String(appRow.modelData.displayText)

                    function activate() {
                        if (root.launcherAccess !== null) {
                            root.launcherAccess.activate(
                                String(appRow.modelData.entryId))
                        }
                        root.close()
                    }

                    onClicked: activate()
                    Keys.onReturnPressed: activate()
                    Keys.onEnterPressed: activate()
                    Accessible.onPressAction: activate()

                    contentItem: Row {
                        spacing: 8

                        ShellIcons.Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: String(appRow.modelData.iconName)
                            size: 20
                            fallbackText: String(appRow.modelData.displayText)
                            Accessible.ignored: true
                        }
                        T.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - parent.spacing - 20
                            text: String(appRow.modelData.displayText)
                            color: "#ffeeeeee"
                            elide: Text.ElideRight
                            Accessible.ignored: true
                        }
                    }
                    background: Rectangle {
                        radius: 4
                        color: appRow.hovered ? "#33ffffff" : "transparent"
                    }

                    Accessible.role: Accessible.MenuItem
                    Accessible.name: String(appRow.modelData.displayText)
                }
            }
        }
    }

    // Section titles translate the stable identity the controller publishes;
    // the model owns no user-facing strings (LauncherSection precedent).
    function sectionTitle(identity) {
        switch (String(identity)) {
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
}

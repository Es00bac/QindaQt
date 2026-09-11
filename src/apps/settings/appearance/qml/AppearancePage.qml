// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Appearance is one Settings1 draft with several focused destinations. The
// page navigator must never create separate drafts: Apply/Revert remain one
// route-level boundary even as a user moves between appearance tasks.
T.Page {
    id: root
    objectName: "appearancePage"

    required property var appearanceSettings
    property var navigation: null
    signal closeRequested()

    readonly property bool editorBusy: appearanceSettings.saving
                                     || appearanceSettings.loading
    property string currentDestination: "themes"
    readonly property Item firstFocusTarget: sectionLoader.item !== null
                                             && sectionLoader.item.firstFocusTarget !== undefined
                                             ? sectionLoader.item.firstFocusTarget
                                             : destinationList
    readonly property string draftSummary: appearanceSettings.draftDirty
        ? qsTr("Changes have not been applied")
        : qsTr("All appearance settings are applied")

    title: qsTr("Appearance")

    function selectDestination(destination) {
        root.currentDestination = destination
        formViewport.contentY = 0
        Qt.callLater(() => {
            if (sectionLoader.item !== null
                    && sectionLoader.item.firstFocusTarget !== undefined
                    && sectionLoader.item.firstFocusTarget !== null)
                sectionLoader.item.firstFocusTarget.forceActiveFocus(Qt.TabFocusReason)
        })
    }

    Keys.priority: Keys.BeforeItem
    Keys.onPressed: event => {
        const pageStep = Math.max(1, formViewport.height - Tokens.space["5"])
        if (event.key === Qt.Key_PageDown) {
            formViewport.contentY = Math.min(
                        Math.max(0, formViewport.contentHeight - formViewport.height),
                        formViewport.contentY + pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            formViewport.contentY = Math.max(0, formViewport.contentY - pageStep)
            event.accepted = true
        } else if (event.key === Qt.Key_Home
                   && (event.modifiers & Qt.ControlModifier)) {
            formViewport.contentY = 0
            event.accepted = true
        } else if (event.key === Qt.Key_End
                   && (event.modifiers & Qt.ControlModifier)) {
            formViewport.contentY = Math.max(
                        0, formViewport.contentHeight - formViewport.height)
            event.accepted = true
        }
    }

    Shortcut {
        sequence: "Ctrl+Home"
        enabled: root.visible
        onActivated: formViewport.contentY = 0
    }

    Shortcut {
        sequence: "Ctrl+End"
        enabled: root.visible
        onActivated: formViewport.contentY = Math.max(
                         0, formViewport.contentHeight - formViewport.height)
    }

    background: Rectangle {
        color: Tokens.bg.base
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "appearancePageHeading"
            Layout.fillWidth: true
            text: qsTr("Appearance")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
            // The purpose sentence lives in the accessible description and the
            // tab tooltips; the visible page stays preview-first.
            Accessible.description: qsTr("Choose how your desktop and QindaQt applications look.")
        }

        Label {
            objectName: "appearanceStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appearanceSettings.statusText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: root.appearanceSettings.conflict
                             || root.appearanceSettings.unavailable
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "appearanceError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appearanceSettings.errorText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: Tokens.fg.default
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Label {
            objectName: "appearanceSaveResults"
            Layout.fillWidth: true
            visible: root.appearanceSettings.saveResultsText.length > 0
            text: root.appearanceSettings.saveResultsText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: root.appearanceSettings.saveResultsHaveFailure
                   ? Tokens.fg.default : Tokens.fg.muted
            Accessible.role: root.appearanceSettings.saveResultsHaveFailure
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "appearanceThemeFallback"
            Layout.fillWidth: true
            visible: root.appearanceSettings.fallbackNotice.length > 0
            text: root.appearanceSettings.fallbackNotice
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: Tokens.fg.muted
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Tokens.space["3"]

            // Destinations are tabs, not buttons: one shared rule with an
            // accent indicator under the selected tab. Each tab carries a
            // glyph and a short label; its explanation is the tooltip.
            TabBar {
                id: destinationList
                objectName: "appearanceDestinationList"
                Layout.fillWidth: true
                Accessible.name: qsTr("Appearance settings")

                readonly property var destinations: [
                    { id: "themes", title: qsTr("Themes"), icon: "preferences-desktop-theme",
                      description: qsTr("Window style, colors, and how applications look") },
                    { id: "wallpaper", title: qsTr("Wallpaper"), icon: "preferences-desktop-wallpaper",
                      description: qsTr("Desktop background picture") },
                    { id: "fonts", title: qsTr("Fonts"), icon: "preferences-desktop-font",
                      description: qsTr("Interface font, size, and rendering") }
                ]

                // Same guard as ShellIcons.Icon: no provider or no icon means
                // no source, so a harness without the icon runtime never warns.
                function iconUrl(name) {
                    const dpr = root.Window.window !== null
                        ? Math.max(1, Math.min(4, root.Window.window.devicePixelRatio)) : 1
                    if (!ShellIcons.IconLookup.hasIcon(name, 18, dpr, true))
                        return ""
                    return "image://qindaqt-icon/" + name + "?size=18&scale=" + dpr
                        + "&symbolic=1&color=" + encodeURIComponent(Tokens.fg.default.toString())
                }

                currentIndex: Math.max(0, destinations.findIndex(
                                           entry => entry.id === root.currentDestination))
                onCurrentIndexChanged: {
                    const entry = destinations[currentIndex]
                    if (entry !== undefined && entry.id !== root.currentDestination)
                        root.selectDestination(entry.id)
                }

                Repeater {
                    model: destinationList.destinations

                    delegate: TabButton {
                        id: destinationButton
                        required property var modelData
                        objectName: "appearanceDestination_" + modelData.id
                        text: modelData.title
                        iconSource: destinationList.iconUrl(modelData.icon)
                        accessibleDescription: modelData.description
                        onClicked: root.selectDestination(modelData.id)
                    }
                }
            }

            Flickable {
                id: formViewport
                objectName: "appearanceFormViewport"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentHeight: sectionLoader.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                activeFocusOnTab: false

                T.ScrollBar.vertical: T.ScrollBar {
                    policy: T.ScrollBar.AsNeeded
                    activeFocusOnTab: false
                    Accessible.name: qsTr("Appearance settings scroll position")
                }

                function revealItem(focusedItem) {
                    if (focusedItem === null || focusedItem === undefined
                            || focusedItem === formViewport) {
                        return
                    }
                    let cursor = focusedItem
                    let belongsToForm = false
                    while (cursor !== null && cursor !== undefined) {
                        if (cursor === sectionLoader.item) {
                            belongsToForm = true
                            break
                        }
                        cursor = cursor.parent
                    }
                    if (!belongsToForm) {
                        return
                    }
                    const position = focusedItem.mapToItem(sectionLoader.item, 0, 0)
                    const margin = Tokens.space["2"]
                    const top = position.y - margin
                    const bottom = position.y + focusedItem.height + margin
                    if (top < contentY) {
                        contentY = Math.max(0, top)
                    } else if (bottom > contentY + height) {
                        contentY = Math.min(Math.max(0, contentHeight - height),
                                            bottom - height)
                    }
                }

                function revealActiveFocus() {
                    if (root.Window.window !== null) {
                        revealItem(root.Window.window.activeFocusItem)
                    }
                }

                onHeightChanged: Qt.callLater(revealActiveFocus)

                Loader {
                    id: sectionLoader
                    objectName: "appearanceDestinationPage_" + root.currentDestination
                    width: formViewport.width
                    sourceComponent: root.currentDestination === "themes" ? themesPage
                        : root.currentDestination === "wallpaper" ? wallpaperPage
                        : fontsPage
                }
            }

            Connections {
                target: root.Window.window
                enabled: root.Window.window !== null
                function onActiveFocusItemChanged() {
                    formViewport.revealActiveFocus()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Label {
                objectName: "appearanceDraftSummary"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.draftSummary
                muted: !root.appearanceSettings.draftDirty
                wrapMode: Text.Wrap
                Accessible.name: text
            }

            Button {
                id: displaySettingsButton
                objectName: "appearanceOpenDisplaySettings"
                visible: root.navigation !== null && root.width >= 560
                emphasized: false
                text: qsTr("Display settings")
                accessibleDescription: qsTr("Change display scale and screen layout")
                onClicked: root.navigation.selectRoute("display")
            }

            Button {
                id: retryButton
                objectName: "appearanceRetryButton"
                visible: root.appearanceSettings.unavailable
                available: !root.editorBusy
                text: qsTr("Retry")
                onClicked: root.appearanceSettings.retry()
            }

            Button {
                id: revertButton
                objectName: "appearanceRevertButton"
                visible: root.appearanceSettings.draftDirty
                         && !root.appearanceSettings.saving
                available: root.appearanceSettings.canEdit
                text: qsTr("Revert")
                onClicked: root.appearanceSettings.cancelDraft()
            }

            Button {
                id: applyButton
                objectName: "appearanceApplyButton"
                visible: !root.appearanceSettings.saving
                available: root.appearanceSettings.applyAvailable
                emphasized: true
                text: qsTr("Apply")
                onClicked: root.appearanceSettings.applyDraft()
            }

        }
    }

    Component {
        id: themesPage
        AppearanceThemeSection {
            appearanceSettings: root.appearanceSettings
            editorBusy: root.editorBusy
        }
    }
    Component {
        id: wallpaperPage
        AppearanceWallpaperSection {
            appearanceSettings: root.appearanceSettings
            editorBusy: root.editorBusy
        }
    }
    Component {
        id: fontsPage
        AppearanceFontSection {
            appearanceSettings: root.appearanceSettings
            editorBusy: root.editorBusy
        }
    }
}

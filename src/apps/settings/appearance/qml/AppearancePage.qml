// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

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
    readonly property bool compactDestinations: width < 720
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
        }

        Label {
            objectName: "appearancePagePurpose"
            Layout.fillWidth: true
            text: qsTr("Choose how your desktop and QindaQt applications look.")
            muted: true
            wrapMode: Text.Wrap
            Accessible.name: text
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Tokens.space["4"]

            ListView {
                id: destinationList
                objectName: "appearanceDestinationList"
                Layout.preferredWidth: root.compactDestinations ? 0 : 176
                Layout.fillHeight: true
                visible: !root.compactDestinations
                clip: true
                spacing: Tokens.space["1"]
                model: [
                    { id: "themes", title: qsTr("Themes"), description: qsTr("Color and window style") },
                    { id: "wallpaper", title: qsTr("Wallpaper"), description: qsTr("Desktop background") },
                    { id: "fonts", title: qsTr("Fonts"), description: qsTr("Text appearance") }
                ]
                Accessible.role: Accessible.PageTabList
                Accessible.name: qsTr("Appearance settings")

                delegate: Button {
                    id: destinationButton
                    required property var modelData
                    objectName: "appearanceDestination_" + modelData.id
                    width: destinationList.width
                    text: modelData.title
                    emphasized: root.currentDestination === modelData.id
                    accessibleDescription: modelData.description
                    Accessible.role: Accessible.PageTab
                    Accessible.selected: root.currentDestination === modelData.id
                    onClicked: root.selectDestination(modelData.id)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Tokens.space["2"]

                ListView {
                    id: compactDestinationList
                    objectName: "appearanceCompactDestinationList"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    visible: root.compactDestinations
                    orientation: ListView.Horizontal
                    clip: true
                    spacing: Tokens.space["1"]
                    model: destinationList.model
                    Accessible.role: Accessible.PageTabList
                    Accessible.name: qsTr("Appearance settings")

                    delegate: Button {
                        required property var modelData
                        objectName: "appearanceCompactDestination_" + modelData.id
                        text: modelData.title
                        emphasized: root.currentDestination === modelData.id
                        accessibleDescription: modelData.description
                        Accessible.role: Accessible.PageTab
                        Accessible.selected: root.currentDestination === modelData.id
                        onClicked: root.selectDestination(modelData.id)
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

                    Loader {
                        id: sectionLoader
                        objectName: "appearanceDestinationPage_" + root.currentDestination
                        width: formViewport.width
                        sourceComponent: root.currentDestination === "themes" ? themesPage
                            : root.currentDestination === "wallpaper" ? wallpaperPage
                            : fontsPage
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Item { Layout.fillWidth: true }

            Label {
                objectName: "appearanceDraftSummary"
                text: root.draftSummary
                muted: !root.appearanceSettings.draftDirty
                Accessible.name: text
            }

            Button {
                id: displaySettingsButton
                objectName: "appearanceOpenDisplaySettings"
                visible: root.navigation !== null
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

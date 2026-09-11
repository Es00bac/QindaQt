// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Input route page: three destinations (Mouse & touchpad, Keyboard,
// Shortcuts) behind the same tab pattern the Appearance page uses. The
// sections own their models; this page only hosts and navigates.
T.Page {
    id: root

    required property var inputSettings
    signal closeRequested()

    property string currentDestination: "pointers"
    readonly property Item firstFocusTarget: sectionLoader.item !== null
                                             && sectionLoader.item.firstFocusTarget !== undefined
                                             ? sectionLoader.item.firstFocusTarget
                                             : destinationBar
    readonly property var destinations: [
        { id: "pointers", title: qsTr("Mouse & touchpad"), icon: "preferences-desktop-peripherals",
          description: qsTr("Pointer speed, scrolling, and touchpad behavior") },
        { id: "keyboard", title: qsTr("Keyboard"), icon: "preferences-desktop-keyboard",
          description: qsTr("Key repeat, NumLock, and keyboard layouts") },
        { id: "shortcuts", title: qsTr("Shortcuts"), icon: "preferences-desktop-keyboard-shortcuts",
          description: qsTr("Global shortcuts and custom command keys") }
    ]

    title: qsTr("Input")
    background: Rectangle { color: Tokens.bg.base }

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
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "inputPageHeading"
            Layout.fillWidth: true
            text: qsTr("Input")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
            Accessible.description: qsTr("Change how pointers, keyboards, and global shortcuts behave.")
        }

        // Same tab contract as the Appearance page: one accent-indicator
        // rule, glyph plus short label per tab, explanation in the tooltip.
        TabBar {
            id: destinationBar
            objectName: "inputDestinationList"
            Layout.fillWidth: true
            Accessible.name: qsTr("Input settings")

            currentIndex: Math.max(0, root.destinations.findIndex(
                                       entry => entry.id === root.currentDestination))
            onCurrentIndexChanged: {
                const entry = root.destinations[currentIndex]
                if (entry !== undefined && entry.id !== root.currentDestination)
                    root.selectDestination(entry.id)
            }

            Repeater {
                model: root.destinations

                delegate: TabButton {
                    id: destinationButton
                    required property var modelData
                    objectName: "inputDestination_" + modelData.id
                    text: modelData.title
                    Accessible.name: modelData.title
                    Accessible.description: modelData.description
                    onClicked: root.selectDestination(modelData.id)
                }
            }
        }

        Flickable {
            id: formViewport
            objectName: "inputFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: sectionLoader.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Input settings scroll position")
            }

            function revealActiveFocus() {
                if (root.Window.window === null) return
                const focused = root.Window.window.activeFocusItem
                if (focused === null || focused === undefined
                        || focused === formViewport) return
                const position = focused.mapToItem(sectionLoader.item, 0, 0)
                const margin = Tokens.space["2"]
                if (position.y < contentY) {
                    contentY = Math.max(0, position.y - margin)
                } else if (position.y + focused.height
                           > contentY + height) {
                    contentY = Math.max(0, Math.min(
                        contentHeight - height,
                        position.y + focused.height - height + margin))
                }
            }

            Loader {
                id: sectionLoader
                objectName: "inputDestinationPage_" + root.currentDestination
                width: formViewport.width
                sourceComponent: root.currentDestination === "keyboard"
                                 ? keyboardPage
                                 : root.currentDestination === "shortcuts"
                                   ? shortcutsPage : pointersPage
                onLoaded: item.forceActiveFocus(Qt.TabFocusReason)
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

    Component {
        id: pointersPage
        InputPointerSection {
            inputSettings: root.inputSettings
        }
    }
    Component {
        id: keyboardPage
        InputKeyboardSection {
            inputSettings: root.inputSettings
        }
    }
    Component {
        id: shortcutsPage
        InputShortcutsSection {
            inputSettings: root.inputSettings
        }
    }
}

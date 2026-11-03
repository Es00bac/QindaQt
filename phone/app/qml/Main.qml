// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Window
import QindaTK as Tk

// QindaLink phone shell: one page at a time above a bottom tab bar.
// AGENT-CONTRACT: the context property `link` (LinkBridge in the app, FakeBridge in
// phone/preview) is the only engine access. `page` is also set by the preview host to grab every
// screen; keep it a plain string property.
Window {
    id: win
    visible: true
    color: Tk.Theme.color.bg
    title: "QindaLink"

    property string page: "home"

    // AGENT-GUARD: the vault shows secrets, so FLAG_SECURE (blank screenshots, recording and recents
    // thumbnail) follows the page; `active` starts the vault poll only while its page is up.
    // `vault` is VaultBridge in the app and FakeVault in the preview.
    onPageChanged: vault.setSecure(page === "vault")
    Binding { target: vault; property: "active"; value: win.page === "vault" }

    // Tab -> pages it owns; the four tools pages keep the Tools tab lit.
    readonly property var tabs: [
        { text: qsTr("Home"), iconName: "house", value: "home", objectName: "tabHome" },
        { text: qsTr("Pairing"), iconName: "link", value: "pairing", objectName: "tabPairing" },
        { text: qsTr("Setup"), iconName: "settings", value: "permissions", objectName: "tabPermissions" },
        { text: qsTr("Tools"), iconName: "layout-grid", value: "tools", objectName: "tabTools" }
    ]
    readonly property var toolPages: ["tools", "touchpad", "keyboard", "macros", "vault"]
    readonly property int tabIndex: {
        if (toolPages.indexOf(page) >= 0) return 3
        for (let i = 0; i < 3; ++i) if (tabs[i].value === page) return i
        return 0
    }

    // AGENT-NOTE: theme and density are process-wide singletons (QindaTK docs/theming.md); the app
    // sets them from QML so it needs no QindaTK C++ header.
    Component.onCompleted: {
        Tk.Theme.applyPreset("sloom-dark")
        Tk.Density.mode = Tk.Density.Touch
    }

    // Walk to the code screen when the engine asks for a comparison.
    Connections {
        target: link
        function onChanged() { if (link.phase === "confirm") win.page = "pairing" }
    }

    // Edge to edge window: keep content clear of the status and navigation bars.
    readonly property real insetTop: win.SafeArea.margins.top
    readonly property real insetBottom: win.SafeArea.margins.bottom

    Component { id: homeC; HomeScreen { onOpenPage: p => win.page = p } }
    Component { id: pairingC; PairingScreen { } }
    Component { id: permissionsC; PermissionsScreen { } }
    Component { id: toolsC; ToolsScreen { onOpenPage: p => win.page = p } }
    Component { id: touchpadC; TouchpadScreen { onBackRequested: win.page = "tools" } }
    Component { id: keyboardC; KeyboardScreen { onBackRequested: win.page = "tools" } }
    Component { id: macrosC; MacrosScreen { onBackRequested: win.page = "tools" } }
    Component { id: vaultC; VaultScreen { onBackRequested: win.page = "tools" } }
    readonly property var components: ({
        home: homeC, pairing: pairingC, permissions: permissionsC, tools: toolsC,
        touchpad: touchpadC, keyboard: keyboardC, macros: macrosC, vault: vaultC })

    Tk.Flex {
        objectName: "appRoot"
        anchors.fill: parent
        anchors.topMargin: win.insetTop
        anchors.bottomMargin: win.insetBottom
        direction: Tk.Flex.Column

        Loader {
            objectName: "pageLoader"
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            sourceComponent: win.components[win.page]
            onLoaded: item.anchors.fill = this
        }
        Tk.Divider { Tk.Flex.alignSelf: Tk.Flex.Stretch }
        Tk.Flex {
            direction: Tk.Flex.Row
            justify: Tk.Flex.Center
            padding: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.PillSwitcher {
                objectName: "tabBar"
                model: win.tabs
                currentIndex: win.tabIndex
                onActivated: (index, value) => win.page = value
            }
        }
    }
}

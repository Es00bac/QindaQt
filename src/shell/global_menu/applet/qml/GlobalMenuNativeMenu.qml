// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Menu {
    id: menu

    required property var menuData
    required property var access
    required property var theme
    property int sourceIndex: -1
    property int depth: 0
    property int maximumDepth: 6
    property bool interactive: true
    property bool projectionReady: false
    readonly property var colors: theme.colors ?? ({})

    objectName: "globalMenuNativeMenu"
    title: String(menuData.text ?? "")
    enabled: interactive && Boolean(menuData.enabled)
        && (menuData.children ?? []).length > 0
    popupType: Popup.Window
    modal: false
    focus: true
    padding: 4
    width: 240
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                 | Popup.CloseOnReleaseOutsideParent

    // The menu tree is an immutable facade publication. Re-publication
    // destroys and recreates its top-level Menu, so dynamically created
    // descendants cannot retain an obsolete generation. Qt owns all popup
    // state after this one-time projection.
    function populate() {
        const children = menuData.children ?? []
        for (let index = 0; index < children.length; ++index) {
            const entry = children[index]
            const kind = String(entry.kind ?? "action")
            if (kind === "submenu" && depth + 1 < maximumDepth) {
                const component = Qt.createComponent(
                    Qt.resolvedUrl("GlobalMenuNativeMenu.qml"),
                    Component.PreferSynchronous)
                if (component.status !== Component.Ready) {
                    console.warn("Unable to create application submenu:",
                                 component.errorString())
                    continue
                }
                const submenu = component.createObject(menu, {
                    "menuData": entry,
                    "access": access,
                    "theme": theme,
                    "depth": depth + 1,
                    "maximumDepth": maximumDepth,
                    "interactive": Qt.binding(function() {
                        return menu.interactive
                    })
                })
                if (submenu !== null)
                    menu.insertMenu(menu.count, submenu)
                continue
            }
            if (kind === "separator") {
                const separator = separatorComponent.createObject(menu.contentItem)
                if (separator !== null)
                    menu.insertItem(menu.count, separator)
                continue
            }
            const item = actionComponent.createObject(menu.contentItem, {
                "entryData": entry,
                "access": access,
                "colors": colors,
                "interactive": kind !== "submenu"
                    ? Qt.binding(function() { return menu.interactive }) : false
            })
            if (item !== null)
                menu.insertItem(menu.count, item)
        }
    }

    function clearProjection() {
        while (count > 0) {
            const submenu = menuAt(0)
            if (submenu !== null)
                removeMenu(submenu)
            else
                removeItem(itemAt(0))
        }
    }

    function rebuildProjection() {
        if (!projectionReady)
            return
        // AGENT-GUARD: Instantiator may reuse a top-level Menu delegate when
        // a new facade publication has the same shape. Dismiss before
        // replacing descendants so no queued gesture can invoke the prior
        // publication generation, then rebuild synchronously from new truth.
        dismiss()
        clearProjection()
        populate()
    }

    onMenuDataChanged: rebuildProjection()
    Component.onCompleted: {
        projectionReady = true
        populate()
    }

    delegate: GlobalMenuNativeMenuItem {
        entryData: subMenu !== null ? subMenu.menuData : ({})
        access: menu.access
        colors: menu.colors
        interactive: menu.interactive
    }

    background: Rectangle {
        radius: menu.theme.cornerRadius ?? 6
        color: menu.colors.surfaceRaised ?? "#2c312e"
        border.color: menu.colors.border ?? "#3c433f"
        border.width: 1
    }

    Component {
        id: actionComponent
        GlobalMenuNativeMenuItem {}
    }

    Component {
        id: separatorComponent
        MenuSeparator {
            objectName: "globalMenuNativeSeparator"
            implicitHeight: 9
            contentItem: Rectangle {
                implicitHeight: 1
                color: menu.colors.border ?? "#3c433f"
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QindaQt.Tokens 1.0

// App-owned actions are rendered by the desktop. Qt owns native popup placement,
// submenu traversal and focus; no executable command or bus address enters QML.
Basic.Menu {
    id: menu
    required property var access
    required property var targetItem
    required property var menuData
    required property string revision
    property string status: "ready"
    property int depth: 0
    property bool projectionReady: false
    property bool rebuilding: false
    property string publishedShape: ""
    property bool restoring: false
    readonly property bool interactive: access !== null && access.activateGranted === true
                                        && status === "ready"
    title: String(menuData.label ?? "")
    popupType: Popup.Window
    modal: false
    focus: true
    padding: 4
    width: 300
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                 | Popup.CloseOnReleaseOutsideParent

    function populate() {
        const entries = menuData.entries ?? menuData.children ?? []
        for (let index = 0; index < entries.length; ++index) {
            const entry = entries[index]
            if (entry.visible === false)
                continue
            if (entry.kind === "submenu" && depth < 6) {
                const component = Qt.createComponent(Qt.resolvedUrl("StatusNotifierMenu.qml"),
                                                     Component.PreferSynchronous)
                const submenu = component.createObject(menu, {
                    access: access, targetItem: targetItem, menuData: entry,
                    revision: revision, depth: depth + 1,
                    status: Qt.binding(function() { return menu ? menu.status : "none" })
                })
                if (submenu)
                    insertMenu(count, submenu)
            } else if (entry.kind === "separator") {
                insertItem(count, separatorComponent.createObject(contentItem))
            } else {
                insertItem(count, actionComponent.createObject(contentItem, {
                    entryData: entry, access: access, targetItem: targetItem,
                    revision: revision,
                    interactive: Qt.binding(function() { return menu ? menu.interactive : false })
                }))
            }
        }
        if (count === 0) {
            insertItem(0, messageComponent.createObject(contentItem, {
                text: status === "loading" ? qsTr("Loading menu…")
                      : status === "error" ? qsTr("Application menu is unavailable")
                      : qsTr("No menu actions")
            }))
        }
    }

    function projectionShape() {
        const entries = menuData.entries ?? menuData.children ?? []
        return revision + ":" + JSON.stringify(menuData) + (entries.length === 0 ? status : "")
    }

    function captureNavigation() {
        const selected = itemAt(currentIndex)
        const submenu = menuAt(currentIndex)
        return { id: selected && selected.entryData ? selected.entryData.id : -1,
                 child: submenu && submenu.opened ? submenu.captureNavigation() : null }
    }

    function restoreNavigation(state) {
        currentIndex = 0
        for (let index = 0; index < count; ++index) {
            const entry = itemAt(index)
            if (entry && entry.entryData && entry.entryData.id === state.id) {
                currentIndex = index
                const submenu = menuAt(index)
                if (state.child && submenu && entry.enabled) {
                    // This is the response to an earlier AboutToShow, not a new
                    // opening gesture. Refetching here would create a loop.
                    submenu.restoring = true
                    submenu.open()
                    submenu.restoreNavigation(state.child)
                }
                return
            }
        }
    }

    function rebuild() {
        if (!projectionReady || rebuilding)
            return
        const shape = projectionShape()
        if (shape === publishedShape && count > 0)
            return
        rebuilding = true
        const navigation = captureNavigation()
        const wasOpen = opened
        // Captured action revisions never change. Retire old delegates before
        // exposing new entries so queued clicks cannot gain new authority.
        while (count > 0) {
            const submenu = menuAt(0)
            if (submenu !== null) {
                removeMenu(submenu)
            } else {
                const oldItem = itemAt(0)
                removeItem(oldItem)
            }
        }
        populate()
        publishedShape = shape
        if (wasOpen)
            restoreNavigation(navigation)
        rebuilding = false
    }

    // An owned timer coalesces a publication and dies with its popup. A queued
    // free JavaScript callback can outlive the delegate on item retirement.
    Timer { id: rebuildTimer; interval: 0; onTriggered: menu.rebuild() }
    onMenuDataChanged: rebuildTimer.restart()
    onRevisionChanged: rebuildTimer.restart()
    onStatusChanged: rebuildTimer.restart()
    onAboutToShow: {
        if (restoring) {
            restoring = false
        } else if (depth > 0 && targetItem && interactive) {
            // Qt can finish opening a retired submenu while its replacement is
            // being installed. Do not turn that stale popup event into a request.
            const current = access.menuStateFor(targetItem.uniqueName, targetItem.objectPath,
                                                targetItem.generation)
            if (String(current.revision) === revision)
                access.aboutToShowMenu(targetItem.uniqueName, targetItem.objectPath,
                                      targetItem.generation, revision, Number(menuData.id))
        }
    }
    onOpened: {
        if (currentIndex < 0)
            currentIndex = 0
        contentItem.forceActiveFocus(Qt.PopupFocusReason)
    }
    Component.onCompleted: {
        projectionReady = true
        populate()
        publishedShape = projectionShape()
    }

    delegate: StatusNotifierMenuItem {
        entryData: subMenu !== null ? subMenu.menuData : ({})
        access: menu ? menu.access : null
        targetItem: menu ? menu.targetItem : null
        revision: menu ? menu.revision : "0"
        interactive: menu ? menu.interactive : false
    }
    background: Rectangle {
        radius: Tokens.radius.m
        color: Tokens.bg.raised
        border.color: Tokens.outline.divider
        border.width: 1
    }
    Component { id: actionComponent; StatusNotifierMenuItem {} }
    Component {
        id: separatorComponent
        Basic.MenuSeparator {
            objectName: "statusNotifierMenuSeparator"
            implicitHeight: 9
            contentItem: Rectangle { implicitHeight: 1; color: Tokens.outline.divider }
        }
    }
    Component {
        id: messageComponent
        Basic.MenuItem { enabled: false; objectName: "statusNotifierMenuMessage" }
    }
}

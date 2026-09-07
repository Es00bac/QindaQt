// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic

// AGENT-NOTE: consumes only GlobalMenuAppletAccess's public Q_PROPERTY/
// Q_INVOKABLE surface (see applet/include/.../globalmenuappletaccess.h).
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    property int maximumVisibleEntries: 8
    readonly property int clampedEntryLimit: Math.max(1, Math.floor(maximumVisibleEntries))
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null && Boolean(access.available)
    readonly property var topLevelItems: access !== null ? (access.items ?? []) : []
    readonly property bool hasContent: topLevelItems.length > 0
    readonly property int effectiveLimit: Math.min(clampedEntryLimit,
        vertical ? verticalLimitFor(height) : horizontalLimitFor(width))
    readonly property var visibleEntries: topLevelItems.slice(0, effectiveLimit)
    readonly property var visibleSubmenus: submenuProjection()
    readonly property int overflowCount: topLevelItems.length - visibleEntries.length
    readonly property bool indicatorFits: vertical
        ? height >= measuredIndicatorHeight() + 4
        : width >= measuredIndicatorWidth() + spacing
    readonly property real spacing: 12
    readonly property real entryGap: vertical ? 4 : spacing
    readonly property real entriesExtent: extentFor(visibleEntries.length)
    // Exposes the native controller for host-level keyboard integration and
    // focused contract tests; callers must not mutate its menu list.
    readonly property alias menuBar: nativeMenuBar
    property var rendererLeaseAccess: null

    objectName: "globalMenuApplet"
    implicitWidth: hasContent ? (vertical ? 40 : naturalHorizontalExtent()) : 0
    implicitHeight: hasContent ? (vertical ? naturalVerticalExtent() : 28) : 0
    clip: true
    Accessible.role: Accessible.MenuBar
    Accessible.name: available ? qsTr("Application menu")
                               : (hasContent ? qsTr("Application menu updating")
                                             : qsTr("Menu unavailable"))

    function updateRendererLease() {
        if (rendererLeaseAccess === access)
            return
        if (rendererLeaseAccess !== null
                && typeof rendererLeaseAccess.detachRenderer === "function")
            rendererLeaseAccess.detachRenderer()
        rendererLeaseAccess = access
        if (rendererLeaseAccess !== null
                && typeof rendererLeaseAccess.attachRenderer === "function")
            rendererLeaseAccess.attachRenderer()
    }

    function measuredTextWidth(text) {
        const value = String(text ?? "")
        return Math.ceil(Math.max(entryMetrics.advanceWidth(value),
                                  entryMetrics.boundingRect(value).width))
    }

    function measuredEntryWidth(item) {
        // Text can retain a fractional glyph advance after FontMetrics has
        // rounded its corresponding bound. Reserve one pixel for that edge.
        return measuredTextWidth(String(item.text ?? "")) + 16
    }

    function measuredIndicatorWidth() {
        return measuredTextWidth(qsTr("+%1").arg(Math.max(1, topLevelItems.length))) + 2
    }

    function measuredIndicatorHeight() {
        const value = qsTr("+%1").arg(Math.max(1, topLevelItems.length))
        return Math.ceil(Math.max(entryMetrics.height,
                                  entryMetrics.boundingRect(value).height)) + 2
    }

    function horizontalLimitFor(assignedWidth) {
        if (topLevelItems.length === 0)
            return 0
        let allUsed = 0
        for (let i = 0; i < topLevelItems.length; ++i)
            allUsed += measuredEntryWidth(topLevelItems[i]) + (i > 0 ? spacing : 0)
        if (topLevelItems.length <= clampedEntryLimit && allUsed <= assignedWidth)
            return topLevelItems.length
        const indicatorBlock = measuredIndicatorWidth() + spacing
        if (assignedWidth < indicatorBlock)
            return 0
        const budget = assignedWidth - indicatorBlock
        let used = 0
        let count = 0
        for (let i = 0; i < topLevelItems.length; ++i) {
            const need = measuredEntryWidth(topLevelItems[i]) + (count > 0 ? spacing : 0)
            if (used + need > budget)
                break
            used += need
            ++count
        }
        return count
    }

    function verticalLimitFor(assignedHeight) {
        if (topLevelItems.length === 0)
            return 0
        const allUsed = topLevelItems.length * 24
            + Math.max(0, topLevelItems.length - 1) * 4
        if (topLevelItems.length <= clampedEntryLimit && allUsed <= assignedHeight)
            return topLevelItems.length
        const indicatorBlock = measuredIndicatorHeight() + 4
        if (assignedHeight < indicatorBlock)
            return 0
        const budget = assignedHeight - indicatorBlock
        let used = 0
        let count = 0
        for (let i = 0; i < topLevelItems.length; ++i) {
            const need = 24 + (count > 0 ? 4 : 0)
            if (used + need > budget)
                break
            used += need
            ++count
        }
        return count
    }

    function naturalHorizontalExtent() {
        const count = Math.min(topLevelItems.length, clampedEntryLimit)
        let extent = 0
        for (let i = 0; i < count; ++i)
            extent += measuredEntryWidth(topLevelItems[i]) + (i > 0 ? spacing : 0)
        return extent + (topLevelItems.length > count
                         ? measuredIndicatorWidth() + spacing : 0)
    }

    function naturalVerticalExtent() {
        const count = Math.min(topLevelItems.length, clampedEntryLimit)
        return count * 24 + Math.max(0, count - 1) * 4
            + (topLevelItems.length > count ? measuredIndicatorHeight() + 4 : 0)
    }

    // AGENT-CONTRACT: horizontal top-level entries fill the applet's actual
    // hosted height, clamped to 36px, instead of a fixed 24px box. The stock
    // 30px panel hosts this applet in a row shorter than that fixed box, so
    // the entry was *taller* than its host and the centering expression
    // resolved to a negative y: the box hung above and below the row it is
    // painted in, wasting hit area outside the clipped row instead of
    // covering the label. Filling the row (bounded so an unusually thick
    // panel cannot grow an oversized target) keeps the whole rendered label
    // clickable edge to edge.
    function horizontalEntryHeight() {
        return Math.max(1, Math.min(height, 36))
    }

    function entryOffset(index) {
        let result = 0
        for (let i = 0; i < index; ++i)
            result += (vertical ? 24 : measuredEntryWidth(visibleEntries[i])) + entryGap
        return result
    }

    function extentFor(count) {
        if (count <= 0)
            return 0
        const last = count - 1
        return entryOffset(last)
            + (vertical ? 24 : measuredEntryWidth(visibleEntries[last]))
    }

    function submenuProjection() {
        const result = []
        for (let i = 0; i < visibleEntries.length; ++i) {
            if (String(visibleEntries[i].kind ?? "action") === "submenu")
                result.push({"entry": visibleEntries[i], "sourceIndex": i})
        }
        return result
    }

    function dismissMenus() {
        for (let i = 0; i < nativeMenuBar.count; ++i) {
            const menu = nativeMenuBar.menuAt(i)
            if (menu !== null)
                menu.dismiss()
        }
    }

    function focusFirstMenuItem(menu) {
        Qt.callLater(function() {
            if (menu !== null && menu.opened && menu.count > 0) {
                for (let index = 0; index < menu.count; ++index) {
                    const item = menu.itemAt(index)
                    // Separators have no triggered signal; disabled actions
                    // must not consume the initial keyboard selection.
                    if (item !== null && item.enabled
                            && typeof item.triggered === "function") {
                        menu.currentIndex = index
                        item.forceActiveFocus(Qt.PopupFocusReason)
                        return
                    }
                }
            }
        })
    }

    onAccessChanged: updateRendererLease()
    onAvailableChanged: {
        if (!available)
            dismissMenus()
    }
    Component.onCompleted: updateRendererLease()
    Component.onDestruction: {
        if (rendererLeaseAccess !== null
                && typeof rendererLeaseAccess.detachRenderer === "function")
            rendererLeaseAccess.detachRenderer()
        rendererLeaseAccess = null
    }

    FontMetrics {
        id: entryMetrics
        font.pixelSize: 12
    }

    Item {
        id: nativeLayout
        objectName: root.vertical ? "globalMenuVerticalLayout"
                                  : "globalMenuHorizontalLayout"
        anchors.fill: parent
        visible: root.hasContent

        // AGENT-CONTRACT: Qt Quick Controls owns menu-open state, switching,
        // popup lifetime, and keyboard traversal. MenuBarItem switches on the
        // pointer press, before an existing popup grab can consume the later
        // release. Do not layer a second open/close controller on this MenuBar.
        Basic.MenuBar {
            id: nativeMenuBar
            objectName: "globalMenuNativeMenuBar"
            anchors.fill: parent
            focusPolicy: Qt.TabFocus
            padding: 0
            spacing: 0
            background: Item {}
            contentItem: Item {}
            Keys.onDownPressed: event => {
                if (count > 0) {
                    itemAt(0).triggered()
                    root.focusFirstMenuItem(menuAt(0))
                    event.accepted = true
                }
            }

            delegate: Basic.MenuBarItem {
                id: menuEntry
                objectName: "globalMenuTopLevelItem"
                readonly property var entryData: menu !== null ? menu.menuData : ({})
                readonly property int sourceIndex: menu !== null ? menu.sourceIndex : -1
                readonly property bool itemEnabled: Boolean(entryData.enabled)
                    && (entryData.children ?? []).length > 0

                function pressAction() {
                    if (enabled)
                        triggered()
                }

                x: root.vertical
                    ? Math.round((nativeMenuBar.width - width) / 2)
                    : root.entryOffset(sourceIndex)
                y: root.vertical ? root.entryOffset(sourceIndex)
                                 : Math.round((nativeMenuBar.height - height) / 2)
                enabled: itemEnabled && root.available
                hoverEnabled: true
                // Keep text padding equal to the measured fifteen-pixel
                // budget; ambient Basic defaults reserve twenty-eight pixels.
                padding: 0
                leftPadding: 7
                rightPadding: 8
                implicitWidth: root.measuredEntryWidth(entryData)
                implicitHeight: root.vertical ? 24 : root.horizontalEntryHeight()
                // MenuBar's internal content layout may assign a narrower
                // width even though this applet positions entries itself.
                width: implicitWidth
                height: implicitHeight
                focusPolicy: Qt.TabFocus
                Accessible.role: Accessible.MenuItem
                Accessible.focusable: enabled
                Accessible.name: String(entryData.text ?? "")
                Accessible.description: qsTr("Opens submenu")
                Accessible.onPressAction: pressAction()
                Keys.onDownPressed: event => {
                    pressAction()
                    root.focusFirstMenuItem(menu)
                    event.accepted = true
                }

                contentItem: Text {
                    text: String(menuEntry.entryData.text ?? "")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: menuEntry.itemEnabled
                        ? (root.colors.text ?? "white")
                        : (root.colors.textMuted ?? "#a9afa9")
                    font: entryMetrics.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: menuEntry.highlighted
                        ? (root.colors.surfaceRaised ?? "#2c312e") : "transparent"
                    radius: 4
                }
            }

            Instantiator {
                model: root.visibleSubmenus
                delegate: GlobalMenuNativeMenu {
                    required property var modelData
                    menuData: modelData.entry
                    sourceIndex: Number(modelData.sourceIndex)
                    access: root.access
                    theme: root.theme
                    interactive: root.available
                    maximumDepth: 6
                }
                onObjectAdded: (index, object) => nativeMenuBar.insertMenu(index, object)
                onObjectRemoved: (index, object) => nativeMenuBar.removeMenu(object)
            }
        }

        Repeater {
            model: root.visibleEntries

            delegate: AbstractButton {
                id: actionEntry
                required property var modelData
                required property int index
                readonly property bool isAction:
                    String(modelData.kind ?? "action") === "action"
                readonly property bool itemEnabled: Boolean(modelData.enabled)

                function pressAction() {
                    if (enabled)
                        root.access.activate(String(modelData.id ?? ""),
                                             String(modelData.generation ?? ""))
                }

                objectName: "globalMenuTopLevelItem"
                visible: isAction
                x: root.vertical ? Math.round((nativeLayout.width - width) / 2)
                                 : root.entryOffset(index)
                y: root.vertical ? root.entryOffset(index)
                                 : Math.round((nativeLayout.height - height) / 2)
                enabled: visible && itemEnabled && root.available
                implicitWidth: root.measuredEntryWidth(modelData)
                implicitHeight: root.vertical ? 24 : root.horizontalEntryHeight()
                focusPolicy: Qt.TabFocus
                checkable: false
                checked: Boolean(modelData.checked ?? false)
                Accessible.role: Accessible.MenuItem
                Accessible.focusable: enabled
                Accessible.name: String(modelData.text ?? "")
                Accessible.checkable: Boolean(modelData.checkable ?? false)
                Accessible.checked: Boolean(modelData.checked ?? false)
                // Press activation is intentional: an already-open native
                // popup may consume the corresponding release at its grab
                // boundary. The pressed edge occurs exactly once for pointer
                // and keyboard gestures, matching MenuBarItem itself.
                onPressedChanged: {
                    if (pressed)
                        pressAction()
                }
                Accessible.onPressAction: pressAction()
                Keys.onReturnPressed: pressAction()
                Keys.onEnterPressed: pressAction()

                contentItem: Text {
                    text: String(actionEntry.modelData.text ?? "")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: actionEntry.itemEnabled
                        ? (root.colors.text ?? "white")
                        : (root.colors.textMuted ?? "#a9afa9")
                    font: entryMetrics.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }
        }
    }

    Text {
        id: overflowIndicator
        objectName: "globalMenuOverflowIndicator"
        visible: root.hasContent && root.overflowCount > 0 && root.indicatorFits
        text: qsTr("+%1").arg(root.overflowCount)
        textFormat: Text.PlainText
        color: root.colors.textMuted ?? "#a9afa9"
        font: entryMetrics.font
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("%1 more menu entries").arg(root.overflowCount)
        x: root.vertical ? Math.round((root.width - width) / 2)
                         : root.entriesExtent + root.spacing
        y: root.vertical ? root.entriesExtent + 4
                         : Math.round((root.height - height) / 2)
    }
}

// AGENT-NOTE: consumes only GlobalMenuAppletAccess's public Q_PROPERTY/
// Q_INVOKABLE surface (see applet/include/.../globalmenuappletaccess.h).

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    // Hard cap on presented entries. Values below 1 are clamped so a negative
    // count can never turn slice()'s negative-index semantics into "show
    // everything".
    property int maximumVisibleEntries: 8
    readonly property int clampedEntryLimit: Math.max(1, Math.floor(maximumVisibleEntries))
    readonly property var colors: theme.colors ?? ({
    })
    readonly property bool available: access !== null && Boolean(access.available)
    // Presentation is decoupled from activation authority: while the shell
    // proves the next provider (phase "loading") the retained entries stay
    // painted, dimmed and disabled, so the panel slot never collapses and
    // reflows mid-swap. Only genuinely empty state collapses to zero extent.
    readonly property var topLevelItems: access !== null ? (access.items ?? []) : []
    readonly property bool hasContent: topLevelItems.length > 0
    readonly property int effectiveLimit: Math.min(clampedEntryLimit, vertical ? verticalLimitFor(height) : horizontalLimitFor(width))
    readonly property var visibleEntries: topLevelItems.slice(0, effectiveLimit)
    readonly property int overflowCount: topLevelItems.length - visibleEntries.length
    // The indicator must fit its own measured size including margins; otherwise it
    // is hidden rather than painted partially inside the clipped geometry.
    readonly property bool indicatorFits: vertical ? height >= (measuredIndicatorHeight() + 4) : width >= (measuredIndicatorWidth() + spacing)
    readonly property real spacing: 12
    property var rendererLeaseAccess: null

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
    onAccessChanged: updateRendererLease()
    Component.onCompleted: updateRendererLease()
    Component.onDestruction: {
        if (rendererLeaseAccess !== null
                && typeof rendererLeaseAccess.detachRenderer === "function")
            rendererLeaseAccess.detachRenderer()
        rendererLeaseAccess = null
    }

    onAvailableChanged: {
        if (!available)
            submenuPopup.close()
    }

    function measuredTextWidth(text) {
        const str = String(text ?? "");
        return Math.ceil(Math.max(entryMetrics.advanceWidth(str), entryMetrics.boundingRect(str).width));
    }

    function measuredEntryWidth(item) {
        // Upper bound of label.implicitWidth (actual painted advance) plus
        // the 12 px entry padding plus a 2 px safety margin.
        return measuredTextWidth(String(item.text ?? "")) + 14;
    }

    function measuredIndicatorWidth() {
        // Worst case: the rendered localized overflow label grows with item count.
        const worstCaseCount = Math.max(1, topLevelItems.length);
        const localizedText = qsTr("+%1").arg(worstCaseCount);
        return measuredTextWidth(localizedText) + 2;
    }

    function measuredIndicatorHeight() {
        const worstCaseCount = Math.max(1, topLevelItems.length);
        const localizedText = qsTr("+%1").arg(worstCaseCount);
        return Math.ceil(Math.max(entryMetrics.height, entryMetrics.boundingRect(localizedText).height)) + 2;
    }

    // Iteratively fit entries (24 px tall, root.spacing apart) while keeping
    // the indicator block inside the assigned axis. Returns 0 when the host
    // is below the documented minimum: the applet then degrades to
    // indicator-only rather than clipping a partial label.
    function horizontalLimitFor(assignedWidth) {
        if (topLevelItems.length === 0)
            return 0;

        let allUsed = 0;
        for (let i = 0; i < topLevelItems.length; ++i) {
            allUsed += measuredEntryWidth(topLevelItems[i]) + (i > 0 ? root.spacing : 0);
        }
        if (topLevelItems.length <= clampedEntryLimit && allUsed <= assignedWidth)
            return topLevelItems.length;

        const indicatorBlock = measuredIndicatorWidth() + root.spacing;
        if (assignedWidth < indicatorBlock)
            return 0;

        const budget = assignedWidth - indicatorBlock;
        let used = 0;
        let count = 0;
        for (let i = 0; i < topLevelItems.length; ++i) {
            const need = measuredEntryWidth(topLevelItems[i]) + (count > 0 ? root.spacing : 0);
            if (used + need > budget)
                break;

            used += need;
            ++count;
        }
        return count;
    }

    function verticalLimitFor(assignedHeight) {
        if (topLevelItems.length === 0)
            return 0;

        const allUsed = topLevelItems.length * 24 + (topLevelItems.length > 0 ? (topLevelItems.length - 1) * 4 : 0);
        if (topLevelItems.length <= clampedEntryLimit && allUsed <= assignedHeight)
            return topLevelItems.length;

        const indicatorBlock = measuredIndicatorHeight() + 4;
        if (assignedHeight < indicatorBlock)
            return 0;

        const budget = assignedHeight - indicatorBlock;
        let used = 0;
        let count = 0;
        for (let i = 0; i < topLevelItems.length; ++i) {
            const need = 24 + (count > 0 ? 4 : 0);
            if (used + need > budget)
                break;

            used += need;
            ++count;
        }
        return count;
    }

    objectName: "globalMenuApplet"
    // AGENT-GUARD: natural size must not depend on width-limited delegates.
    // Otherwise a host using implicitWidth permanently collapses to +N.
    implicitWidth: hasContent ? (vertical ? 40 : naturalHorizontalExtent()) : 0
    implicitHeight: hasContent ? (vertical ? naturalVerticalExtent() : 28) : 0

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
    clip: true
    Accessible.role: Accessible.MenuBar
    Accessible.name: available ? qsTr("Application menu") : (hasContent ? qsTr("Application menu updating") : qsTr("Menu unavailable"))

    // Measured, deterministic geometry contract. AGENT-GUARD: the limit
    // loops must consume strict UPPER bounds of the real rendered sizes
    // (pure FontMetrics measurement plus safety margin), so a retained delegate
    // can never be wider/taller than its budget and the +N indicator is
    // always reserved inside the assigned extent — wide glyphs or exotic
    // fonts cost accuracy, never correctness. Hosts below the documented
    // minimum degrade to indicator-only instead of clipping partial labels.
    FontMetrics {
        id: entryMetrics

        font.pixelSize: 12
    }

    Text {
        id: placeholder

        anchors.centerIn: parent
        visible: false
        text: ""
        textFormat: Text.PlainText
        color: root.colors.textMuted ?? "#a9afa9"
        font.pixelSize: 12
    }

    GlobalMenuPopup {
        id: submenuPopup
        access: root.access
        theme: root.theme
        maximumDepth: 6
    }

    Row {
        id: row

        objectName: "globalMenuHorizontalLayout"
        anchors.verticalCenter: parent.verticalCenter
        visible: root.hasContent && !root.vertical
        spacing: root.spacing

        Repeater {
            model: root.visibleEntries

            delegate: MenuEntry {
            }

        }

    }

    Column {
        id: verticalLayout

        objectName: "globalMenuVerticalLayout"
        anchors.horizontalCenter: parent.horizontalCenter
        visible: root.hasContent && root.vertical
        spacing: 4

        Repeater {
            model: root.visibleEntries

            delegate: MenuEntry {
            }

        }

    }

    Text {
        id: overflowIndicator

        objectName: "globalMenuOverflowIndicator"
        // AGENT-GUARD: below the documented host minimum the indicator hides
        // itself rather than painting partially inside the clipped geometry;
        // the limit loops reserve its measured size whenever it is shown.
        visible: root.hasContent && root.overflowCount > 0 && root.indicatorFits
        text: qsTr("+%1").arg(root.overflowCount)
        textFormat: Text.PlainText
        color: root.colors.textMuted ?? "#a9afa9"
        font.pixelSize: 12
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("%1 more menu entries").arg(root.overflowCount)
        anchors.left: root.vertical ? undefined : row.right
        anchors.leftMargin: root.spacing
        anchors.verticalCenter: root.vertical ? undefined : row.verticalCenter
        anchors.top: root.vertical ? verticalLayout.bottom : undefined
        anchors.topMargin: 4
        anchors.horizontalCenter: root.vertical ? verticalLayout.horizontalCenter : undefined
    }

    component MenuEntry: AbstractButton {
        id: entry

        required property var modelData
        readonly property bool isAction: String(modelData.kind ?? "action") === "action"
        readonly property bool isSubmenu: String(modelData.kind ?? "") === "submenu"
        readonly property bool itemEnabled: Boolean(modelData.enabled)
            && (isAction || (isSubmenu && (modelData.children ?? []).length > 0))

        // AGENT-GUARD: one named activation path is shared by pointer click,
        // keyboard activation, and assistive-technology press. AbstractButton
        // suppresses clicked() and keyboard activation while disabled, but an
        // AT press has no such gate; the explicit enabled check keeps
        // non-activating entries (disabled actions and empty submenus) honest.
        // Menu-bar switch contract: clicking the entry whose menu is open
        // closes it; clicking another entry switches the popup to it. The
        // toggle is judged at clicked() time against the popup's close record
        // — delivery order between the press-outside close and this button's
        // pressed() varies across platforms (and the native popup grab may
        // swallow the press entirely), so a press-time flag is unreliable.
        function pressAction() {
            if (!entry.enabled)
                return
            if (entry.isSubmenu) {
                if (submenuPopup.opened && submenuPopup.anchorItem === entry) {
                    submenuPopup.close()
                    return
                }
                if (!submenuPopup.opened
                        && submenuPopup.closedAnchorItem === entry
                        && (Date.now() - submenuPopup.closedAt) < 400)
                    return
                submenuPopup.openMenu(entry.modelData, entry)
            } else {
                root.access.activate(entry.modelData.id, String(entry.modelData.generation ?? ""))
            }
        }

        objectName: "globalMenuTopLevelItem"
        focusPolicy: Qt.TabFocus
        // Retained entries painted during a provider swap (phase "loading")
        // are inert: disabled buttons ignore pointer/keyboard activation and
        // are skipped by focus, so a stale menu can never act for a new focus.
        enabled: itemEnabled && root.available
        implicitWidth: label.implicitWidth + 12
        implicitHeight: 24
        // AGENT-CONTRACT: presentation never owns toggle state. The button
        // stays non-toggleable so Space/click cannot locally invert `checked`;
        // the provider-owned value is bound into the accessible state, and an
        // activation request lets the provider republish new truth.
        checkable: false
        checked: Boolean(modelData.checked ?? false)
        Accessible.role: Accessible.MenuItem
        Accessible.focusable: entry.enabled
        Accessible.checkable: Boolean(modelData.checkable ?? false)
        Accessible.checked: Boolean(modelData.checked ?? false)
        Accessible.name: String(modelData.text ?? "")
        Accessible.description: isSubmenu ? qsTr("Opens submenu") : ""
        onClicked: entry.pressAction()
        // Classic menu-bar behavior: with a menu open, hovering another
        // entry switches the popup to it. This is also the native switching
        // mechanism when the popup's xdg_popup grab swallows the switching
        // click before it reaches the panel.
        hoverEnabled: true
        onHoveredChanged: {
            if (entry.hovered && entry.enabled && entry.isSubmenu
                    && submenuPopup.opened && submenuPopup.anchorItem !== entry)
                submenuPopup.openMenu(entry.modelData, entry)
        }
        Accessible.onPressAction: entry.pressAction()
        Keys.onReturnPressed: entry.pressAction()
        Keys.onEnterPressed: entry.pressAction()
        Keys.onDownPressed: event => {
            if (entry.isSubmenu) {
                entry.pressAction()
                event.accepted = true
            }
        }

        contentItem: Text {
            id: label

            text: String(entry.modelData.text ?? "")
            textFormat: Text.PlainText
            elide: Text.ElideRight
            maximumLineCount: 1
            color: entry.itemEnabled ? (root.colors.text ?? "white") : (root.colors.textMuted ?? "#a9afa9")
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Item {
        }

    }

}

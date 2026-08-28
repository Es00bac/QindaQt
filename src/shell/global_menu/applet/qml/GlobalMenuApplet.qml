// AGENT-NOTE: consumes only GlobalMenuAppletAccess's public Q_PROPERTY/
// Q_INVOKABLE surface (see applet/include/.../globalmenuappletaccess.h).
// G0 wires no live publisher anywhere in the shell, so `available` stays
// false and this renders the same unavailable placeholder as an unprovisioned
// notification center; do not read this component's presence as a live
// global menu.

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// AGENT-GUARD: top-level "submenu" entries must stay visibly present but
// non-activating (disabled, no activation call) — G0 has no submenu popup,
// and rendering them as clickable fakes would pretend an interaction that
// does not exist. Only enabled "action" entries activate.
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
    readonly property var topLevelItems: available ? (access.items ?? []) : []
    readonly property int effectiveLimit: Math.min(clampedEntryLimit, vertical ? verticalLimitFor(height) : horizontalLimitFor(width))
    readonly property var visibleEntries: topLevelItems.slice(0, effectiveLimit)
    readonly property int overflowCount: topLevelItems.length - visibleEntries.length
    // The indicator must fit its own measured size including margins; otherwise it
    // is hidden rather than painted partially inside the clipped geometry.
    readonly property bool indicatorFits: vertical ? height >= (measuredIndicatorHeight() + 4) : width >= (measuredIndicatorWidth() + spacing)
    readonly property real spacing: 12

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
    implicitWidth: vertical ? 40 : available ? row.implicitWidth + (overflowCount > 0 && indicatorFits ? overflowIndicator.implicitWidth + spacing : 0) : placeholder.implicitWidth + 16
    // AGENT-GUARD: the +N indicator is anchored below the vertical column, so
    // vertical implicit height must include it or the clipped root geometry
    // would hide the affordance the limit exists to surface.
    implicitHeight: vertical ? (available ? verticalLayout.implicitHeight + (overflowCount > 0 && indicatorFits ? overflowIndicator.implicitHeight + 4 : 0) : 28) : 28
    clip: true
    Accessible.role: Accessible.MenuBar
    Accessible.name: available ? qsTr("Application menu") : qsTr("Menu unavailable")

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
        visible: !root.available
        text: qsTr("Menu unavailable")
        textFormat: Text.PlainText
        color: root.colors.textMuted ?? "#a9afa9"
        font.pixelSize: 12
    }

    Row {
        id: row

        objectName: "globalMenuHorizontalLayout"
        anchors.verticalCenter: parent.verticalCenter
        visible: root.available && !root.vertical
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
        visible: root.available && root.vertical
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
        visible: root.available && root.overflowCount > 0 && root.indicatorFits
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
        readonly property bool itemEnabled: isAction && Boolean(modelData.enabled)

        // AGENT-GUARD: one named activation path is shared by pointer click,
        // keyboard activation, and assistive-technology press. AbstractButton
        // suppresses clicked() and keyboard activation while disabled, but an
        // AT press has no such gate; the explicit enabled check keeps
        // non-activating entries (disabled actions, G0 submenus) honest.
        function pressAction() {
            if (entry.enabled)
                root.access.activate(entry.modelData.id);

        }

        objectName: "globalMenuTopLevelItem"
        focusPolicy: Qt.TabFocus
        enabled: itemEnabled
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
        Accessible.name: String(modelData.text ?? "") + (isAction ? "" : qsTr(" (submenu unavailable)"))
        onClicked: entry.pressAction()
        Accessible.onPressAction: entry.pressAction()
        Keys.onReturnPressed: entry.pressAction()
        Keys.onEnterPressed: entry.pressAction()

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

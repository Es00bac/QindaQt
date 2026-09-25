// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaTK as Tk
import "EntryText.js" as EntryText

// The key facts of one entry (ADR-0270), beside the Gallery view's large
// preview and in the Columns view's preview column. A fact the listing or
// EntryFacts does not know is left out rather than shown as zero.
ColumnLayout {
    id: root

    property var entry: null
    property var entryFacts: null
    property bool relativeDates: false
    property bool showExtensions: true
    property bool applicationsPlace: false
    property var locale: Qt.locale()

    readonly property var facts: {
        const entry = root.entry
        if (!entry)
            return []
        const facts = root.entryFacts
        const revision = facts ? facts.revision : 0
        const stamp = String(entry.modifiedNanoseconds || "")
        const image = entry.iconName === "image-x-generic" && entry.isSymlink !== true
        const rows = [
            { "key": root.applicationsPlace ? qsTr("Category") : qsTr("Kind"),
              "value": entry.kindText || "" },
            { "key": qsTr("Size"), "value": entry.isDirectory === true ? "" : entry.sizeText || "" },
            { "key": qsTr("Items"), "value": facts && entry.isDirectory === true
                                             ? facts.itemCount(entry.path, stamp) : "" },
            { "key": qsTr("Dimensions"), "value": facts && image ? facts.dimensions(entry.path, stamp) : "" },
            { "key": qsTr("Modified"), "value": EntryText.dateText(entry.modified, root.relativeDates, root.locale) },
            { "key": qsTr("Created"), "value": EntryText.dateText(entry.created, root.relativeDates, root.locale) },
            { "key": qsTr("Where"), "value": EntryText.folderOf(entry) }
        ]
        return rows.filter(row => revision >= 0 && row.value.length > 0 && row.value !== "—")
    }

    spacing: Tk.Theme.space.xs

    Tk.Label {
        objectName: "entryFactsName"
        Layout.fillWidth: true
        visible: root.entry !== null
        text: root.entry ? EntryText.displayName(root.entry, root.showExtensions) : ""
        font.weight: Font.DemiBold
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideMiddle
    }
    // ADR-0262: why an application cannot start from here, in words.
    Tk.Label {
        Layout.fillWidth: true
        visible: root.entry !== null && (root.entry.note || "").length > 0
        text: root.entry ? (root.entry.note || "") : ""
        muted: true
        wrapMode: Text.Wrap
    }
    Repeater {
        model: root.facts
        delegate: Tk.KeyValue {
            required property var modelData
            Layout.fillWidth: true
            key: modelData.key
            value: modelData.value
            mono: false
        }
    }
    Item { Layout.fillHeight: true }
}

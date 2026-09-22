// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Mirror the selected display onto another one, or extend across both.
//
// Display1 models mirroring as a single field on the mirrored output
// (`replicationSourceStableId`); this row is the only surface that writes it.
// The choices are "Extend" plus one per other enabled, not-itself-mirrored
// output, which is exactly what DisplaySettingsModel::setOutputMirror accepts.
FormRow {
    id: root

    required property var displaySettings
    required property bool editorBusy

    readonly property string selectedId: String(root.displaySettings?.selectedOutputId ?? "")
    readonly property string mirrorSourceId:
        String(root.displaySettings?.selectedOutput?.replicationSourceStableId ?? "")
    readonly property bool outputEnabled: root.displaySettings?.selectedOutput?.enabled ?? false
    readonly property bool mirrorEnabled: (root.displaySettings?.canEdit ?? false) && !root.editorBusy
                                          && root.outputEnabled && root.candidates.length > 0

    // Every other output this one may mirror. A disabled output has no pixels
    // to copy, and an output that is itself mirroring would make the source
    // ambiguous, so both are excluded rather than offered and refused.
    readonly property var candidates: {
        const all = root.displaySettings?.outputs ?? []
        const result = []
        for (let index = 0; index < all.length; ++index) {
            const candidate = all[index]
            if (candidate.stableId === root.selectedId || !candidate.enabled) {
                continue
            }
            if (String(candidate.replicationSourceStableId ?? "") !== "") {
                continue
            }
            result.push(candidate)
        }
        return result
    }

    // The mode the source is showing, so a mismatch can be stated rather than
    // discovered: mirroring drives both panels from one framebuffer, and a
    // panel whose native mode differs will scale or letterbox it.
    readonly property string mismatchNote: {
        if (root.mirrorSourceId === "") {
            return ""
        }
        const all = root.displaySettings?.outputs ?? []
        let source = null
        let target = null
        for (let index = 0; index < all.length; ++index) {
            if (all[index].stableId === root.mirrorSourceId) {
                source = all[index]
            }
            if (all[index].stableId === root.selectedId) {
                target = all[index]
            }
        }
        if (source === null || target === null) {
            return ""
        }
        if (source.logicalWidth === target.logicalWidth
                && source.logicalHeight === target.logicalHeight) {
            return ""
        }
        return qsTr("This display is %1 × %2 and the mirrored one is %3 × %4, so one of them is scaled.")
            .arg(target.logicalWidth).arg(target.logicalHeight)
            .arg(source.logicalWidth).arg(source.logicalHeight)
    }

    Layout.fillWidth: true
    label: qsTr("Mirroring")
    description: root.candidates.length > 0
                 ? (root.mismatchNote !== "" ? root.mismatchNote
                                             : qsTr("Show the same picture on another display"))
                 : qsTr("Connect a second display to mirror onto one")
    editor: mirrorChoiceRow

    Flow {
        id: mirrorChoiceRow
        objectName: "displayMirrorChoiceRow"
        spacing: Tokens.space["2"]

        Button {
            objectName: "displayMirrorExtendButton"
            checkable: true
            autoExclusive: true
            available: root.mirrorEnabled
            text: qsTr("Extend")
            checked: root.mirrorSourceId === ""

            Accessible.role: Accessible.RadioButton
            Accessible.name: qsTr("Extend across displays")
            Accessible.checked: checked

            onClicked: {
                if (root.selectedId !== "") {
                    root.displaySettings?.setOutputMirror(root.selectedId, "")
                }
            }
        }

        Repeater {
            model: root.candidates

            delegate: Button {
                id: mirrorBtn
                required property var modelData

                objectName: "displayMirrorButton_" + mirrorBtn.modelData.connectorName
                checkable: true
                autoExclusive: true
                available: root.mirrorEnabled
                text: qsTr("Mirror onto %1").arg(mirrorBtn.modelData.label)
                checked: root.mirrorSourceId === mirrorBtn.modelData.stableId

                Accessible.role: Accessible.RadioButton
                Accessible.name: text
                Accessible.checked: checked

                onClicked: {
                    if (root.selectedId !== "") {
                        root.displaySettings?.setOutputMirror(
                            root.selectedId, mirrorBtn.modelData.stableId)
                    }
                }
            }
        }
    }
}

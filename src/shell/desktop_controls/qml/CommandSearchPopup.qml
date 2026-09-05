// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Search popup shared by the command palette, the HUD, and the overview: one
// query field over a CommandSearchController facade plus a keyboard-navigable
// ranked result list. Down/Up move between the field and the rows; Return on
// a row (or on the field with results) activates through the facade.
ControlPopupFrame {
    id: popup

    required property var access
    property string placeholderText: qsTr("Search")
    property string emptyText: qsTr("No matches")

    readonly property bool ready: access !== null && Tokens.ready
    readonly property var resultRows: ready ? access.results : []
    property int currentIndex: -1

    objectName: "commandSearchPopup"
    width: 420
    feedback: ready && access.feedbackPresent ? access.feedback : ""
    initialFocusItem: searchField

    function activateIndex(index) {
        if (!ready || index < 0 || index >= resultRows.length)
            return
        const generation = resultRows[index].generation === undefined
                           ? ""
                           : String(resultRows[index].generation)
        if (access.activate(String(resultRows[index].id), generation))
            popup.close()
    }

    function focusRow(index) {
        if (index < 0) {
            searchField.forceActiveFocus(Qt.TabFocusReason)
            return
        }
        if (index >= resultRepeater.count)
            return
        const item = resultRepeater.itemAt(index)
        if (item !== null)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    onClosed: {
        popup.currentIndex = -1
        if (ready)
            access.query = ""
    }

    C.TextField {
        id: searchField
        objectName: "commandSearchField"
        Layout.fillWidth: true
        enabled: popup.ready && popup.access.available
        text: popup.ready ? popup.access.query : ""
        placeholderText: popup.placeholderText
        accessibleName: popup.placeholderText
        accessibleDescription: qsTr("Type to search; press Down to move to the results")
        onTextEdited: if (popup.ready) popup.access.query = text
        Keys.onDownPressed: popup.focusRow(0)
        Keys.onReturnPressed: popup.activateIndex(0)
        Keys.onEnterPressed: popup.activateIndex(0)
    }

    C.Label {
        objectName: "commandSearchUnavailable"
        Layout.fillWidth: true
        visible: popup.ready && !popup.access.available
        text: qsTr("Search sources are unavailable: %1")
              .arg(popup.ready ? popup.access.phaseReasonText : "")
        muted: true
        Accessible.role: Accessible.AlertMessage
    }

    C.Label {
        objectName: "commandSearchEmpty"
        Layout.fillWidth: true
        visible: popup.ready && popup.access.available && popup.resultRows.length === 0
        text: popup.emptyText
        muted: true
    }

    Flickable {
        id: resultFlick
        objectName: "commandSearchResults"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(360, resultColumn.implicitHeight)
        implicitHeight: Math.min(360, resultColumn.implicitHeight)
        contentHeight: resultColumn.implicitHeight
        contentWidth: width
        clip: true
        interactive: contentHeight > height
        Accessible.role: Accessible.List
        Accessible.name: qsTr("%1 results").arg(popup.resultRows.length)

        function reveal(item) {
            if (item === null)
                return
            if (item.y < contentY)
                contentY = item.y
            else if (item.y + item.height > contentY + height)
                contentY = Math.max(0, item.y + item.height - height)
        }

        ColumnLayout {
            id: resultColumn
            width: resultFlick.width
            spacing: Tokens.space["1"]

            Repeater {
                id: resultRepeater
                model: popup.resultRows

                delegate: MenuRow {
                    required property var modelData
                    required property int index

                    objectName: "commandSearchRow"
                    Layout.fillWidth: true
                    text: String(modelData.text)
                    detail: String(modelData.detail)
                    iconName: String(modelData.iconName)
                    enabled: Boolean(modelData.enabled)
                    current: index === popup.currentIndex
                    Accessible.name: String(modelData.accessibleName)
                    onActivated: popup.activateIndex(index)
                    Keys.onUpPressed: popup.focusRow(index - 1)
                    Keys.onDownPressed: popup.focusRow(index + 1)
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            popup.currentIndex = index
                            resultFlick.reveal(this)
                        }
                    }
                }
            }
        }
    }
}

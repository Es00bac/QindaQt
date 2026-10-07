// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

Tk.Dialog {
    id: dialog
    objectName: "viewerTextDialog"
    required property var viewerModel
    title: qsTr("PDF text")
    subtitle: qsTr("Page %1 of %2 · Select text and use Ctrl+C to copy.")
        .arg(viewerModel.page + 1).arg(viewerModel.pageCount)
    dialogWidth: Tk.Theme.size.dialogWidth * 1.5
    primaryText: qsTr("Close")
    secondaryText: ""
    property string appliedMatch: ""
    onOpened: {
        appliedMatch = ""
        query.forceActiveFocus(Qt.ShortcutFocusReason)
        revealMatch()
    }
    onClosed: viewerModel.cancelSearch()

    function revealMatch() {
        if (!visible || !viewerModel.matchReady) return
        const key = viewerModel.page + ":" + viewerModel.matchStart + ":" + viewerModel.matchLength
        if (key === appliedMatch) return
        appliedMatch = key
        Qt.callLater(function() {
            if (!dialog.visible || !dialog.viewerModel.matchReady) return
            pageText.select(dialog.viewerModel.matchStart,
                            dialog.viewerModel.matchStart + dialog.viewerModel.matchLength)
            textScroll.ensureVisible(caret)
        })
    }
    Connections {
        target: dialog.viewerModel
        function onStateChanged() {
            if (!target.ready || !target.pdf || !target.textAllowed) {
                dialog.close()
                return
            }
            if (target.matchStart < 0) dialog.appliedMatch = ""
            dialog.revealMatch()
        }
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        Tk.TextField {
            id: query
            objectName: "viewerFindQuery"
            placeholderText: qsTr("Find in document")
            maximumLength: 512
            Accessible.name: qsTr("Find text in PDF")
            onTextEdited: dialog.viewerModel.cancelSearch()
            Keys.priority: Keys.BeforeItem
            Keys.onReturnPressed: event => {
                if (!dialog.viewerModel.busy && !dialog.viewerModel.searchBusy)
                    dialog.viewerModel.find(query.text, false, matchCase.checked)
                event.accepted = true
            }
            Keys.onEnterPressed: event => {
                if (!dialog.viewerModel.busy && !dialog.viewerModel.searchBusy)
                    dialog.viewerModel.find(query.text, false, matchCase.checked)
                event.accepted = true
            }
        }
        Tk.Flex {
            direction: Tk.Flex.Row
            wrap: true
            gap: Tk.Theme.space.sm
            Tk.CheckBox {
                id: matchCase
                objectName: "viewerFindMatchCase"
                text: qsTr("Match case")
                onToggled: dialog.viewerModel.cancelSearch()
            }
            Tk.Button {
                objectName: "viewerFindPrevious"
                text: qsTr("Previous")
                enabled: query.text.length > 0 && !dialog.viewerModel.busy
                    && !dialog.viewerModel.searchBusy
                onClicked: dialog.viewerModel.find(query.text, true, matchCase.checked)
            }
            Tk.Button {
                objectName: "viewerFindNext"
                text: qsTr("Next")
                enabled: query.text.length > 0 && !dialog.viewerModel.busy
                    && !dialog.viewerModel.searchBusy
                onClicked: dialog.viewerModel.find(query.text, false, matchCase.checked)
            }
            Tk.Button {
                objectName: "viewerFindCancel"
                text: qsTr("Stop search")
                visible: dialog.viewerModel.searchBusy
                onClicked: dialog.viewerModel.cancelSearch()
            }
        }
        Tk.Caption {
            objectName: "viewerFindStatus"
            text: dialog.viewerModel.searchMessage
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
        Tk.Caption {
            objectName: "viewerTextStatus"
            text: dialog.viewerModel.busy ? qsTr("Reading page text…")
                : dialog.viewerModel.textError.length > 0 ? dialog.viewerModel.textError
                : dialog.viewerModel.pageText.length === 0 ? qsTr("This page has no selectable text.") : ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
        Tk.Scroll {
            id: textScroll
            objectName: "viewerTextScroll"
            implicitHeight: Tk.Theme.size.control * 8
            accessibleName: qsTr("PDF page text")
            Tk.TextArea {
                id: pageText
                objectName: "viewerPageText"
                text: dialog.viewerModel.pageText
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                activeFocusOnTab: true
                rows: 8
                Accessible.name: qsTr("Selectable PDF page text")
                onCursorRectangleChanged: textScroll.ensureVisible(caret)
                Item {
                    id: caret
                    x: pageText.cursorRectangle.x
                    y: pageText.cursorRectangle.y
                    width: pageText.cursorRectangle.width
                    height: pageText.cursorRectangle.height
                }
            }
        }
        Tk.Button {
            objectName: "viewerCopyText"
            text: qsTr("Copy selection")
            enabled: pageText.selectedText.length > 0
            onClicked: pageText.copy()
        }
    }
}

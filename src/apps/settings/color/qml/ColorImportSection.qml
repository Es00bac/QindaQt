// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var colorSettings
    // AGENT-NOTE: Import never needs service authority, so the button is the
    // page's always-admitted domain control and a safe focus fallback.
    readonly property Item importTarget: importButton
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Profile catalog")
        description: qsTr("Discovered ICC profiles and user imports")
    }

    Label {
        objectName: "colorCatalogSummary"
        Layout.fillWidth: true
        text: root.colorSettings.catalogSummaryText
        wrapMode: Text.Wrap
        muted: true
        Accessible.name: text
    }

    Label {
        objectName: "colorImportStatus"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.colorSettings.importStatusText
        wrapMode: Text.Wrap
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Button {
        id: importButton
        objectName: "colorImportButton"
        Layout.fillWidth: true
        available: true
        busy: false
        emphasized: false
        text: qsTr("Import profile…")
        accessibleDescription: qsTr("Choose a local ICC profile file and copy it into the user profile directory")
        onClicked: importDialog.open()
    }

    // AGENT-CONTRACT: Bounded local file dialog only — no portal mediation,
    // no remote locations; the C1 import seam revalidates the chosen file
    // fail-closed before anything is stored.
    FileDialog {
        id: importDialog
        title: qsTr("Import ICC profile")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("ICC profiles (*.icc *.icm)"), qsTr("All files (*)")]
        onAccepted: root.colorSettings.importProfile(importDialog.selectedFile)
    }
}

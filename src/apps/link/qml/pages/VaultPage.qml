// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Vault: a placeholder. The vault lives only on the phone (ADR-0433 rule 2); typing a secret into
// a desktop field is configured there.
Item {
    objectName: "vaultPage"
    Tk.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 360)
        maxTextWidth: width
        iconName: "lock"
        title: qsTr("Vault")
        text: qsTr("Vault typing is configured on the phone.")
    }
}

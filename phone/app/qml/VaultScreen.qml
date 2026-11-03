// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Password vault (phone only). AGENT-NOTE: one screen frame hosts a Loader that swaps the views
// (gate, list, detail, edit, backup); each view is a separate Vault*View.qml so a revealed secret
// dies with its view. Everything comes from the context property `vault` (VaultBridge in the app,
// FakeVault in phone/preview). The shell keeps the `openPage`/`backRequested` contract of ScreenFrame.
// AGENT-GUARD: while this page is shown Main.qml sets FLAG_SECURE (vault.setSecure); do not move
// secrets into properties of this root, and keep `touch()` on pointer activity so the 5 minute idle
// lock measures real use.
ScreenFrame {
    id: screen
    objectName: "vaultScreen"

    // Requested view and entry; the preview host sets these to grab each screen.
    property string view: "list"
    property string entryId: ""
    property bool startWithGenerator: false
    readonly property string shown: vault.state === "unlocked" ? view : "gate"
    // AGENT-GUARD: whenever the app is not in front (system prompt, recents, screen off) every view is
    // unloaded, which drops revealed secrets and typed passwords, and the next frame starts at the
    // list or gate, never at a detail or editor. Do not keep views alive while inactive.
    readonly property bool appActive: Qt.application.state === Qt.ApplicationActive
    onAppActiveChanged: if (!appActive) { view = "list"; entryId = ""; startWithGenerator = false }

    title: qsTr("Vault")
    subtitle: vault.state === "unlocked" ? qsTr("%1 saved").arg(vault.entries.length) : ""
    backVisible: shown === "gate" || shown === "list"

    function go(v, id) {
        if (v === "generate") { entryId = ""; startWithGenerator = true; view = "edit"; return }
        startWithGenerator = false
        entryId = id
        view = v
    }

    PointHandler {
        acceptedDevices: PointerDevice.AllDevices
        onActiveChanged: vault.touch()
    }

    Component { id: gateC; VaultGateView { } }
    Component { id: listC; VaultListView { onNavigate: (v, id) => screen.go(v, id) } }
    Component { id: detailC; VaultDetailView { entryId: screen.entryId; onNavigate: (v, id) => screen.go(v, id) } }
    Component {
        id: editC
        VaultEditView {
            entryId: screen.entryId
            withGenerator: screen.startWithGenerator
            onNavigate: (v, id) => screen.go(v, id)
        }
    }
    Component { id: backupC; VaultBackupView { onNavigate: (v, id) => screen.go(v, id) } }
    readonly property var views: ({ gate: gateC, list: listC, detail: detailC, edit: editC, backup: backupC })

    Loader {
        objectName: "vaultLoader"
        active: screen.appActive
        sourceComponent: screen.views[screen.shown] || gateC
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onLoaded: item.width = Qt.binding(() => width)
    }
}

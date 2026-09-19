// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import org.kde.plasma.plasmoid

// AGENT-CONTRACT: this is how a QindaQt screensaver reaches a locked session
// (ADR-0216). The locker draws the saver itself; no screensaver process is ever
// shown above the lock screen, so a saver that hangs can never stand between
// the user and the password prompt. The greeter's own UI fades out after ten
// seconds of inactivity and returns on input, which is what keeps the prompt
// off an idle locked screen without any cooperation from the scene below.
WallpaperItem {
    id: root

    // AGENT-GUARD: the ground must be painted before anything else loads. A
    // saver package that is absent, broken, or slow to start leaves this
    // rectangle on screen; the lock screen never depends on a scene loading.
    Rectangle {
        anchors.fill: parent
        color: "#0b0f0d"
    }

    Loader {
        id: scene
        anchors.fill: parent
        asynchronous: true
        // Each scene lives in its own file so an uninstalled saver package
        // fails this Loader instead of failing the wallpaper itself.
        source: {
            switch (root.configuration.Saver) {
            case "qinda-patrol":
                return Qt.resolvedUrl("PatrolScene.qml")
            case "circuit-reef":
                return Qt.resolvedUrl("ReefScene.qml")
            default:
                return ""
            }
        }
        onStatusChanged: {
            if (status === Loader.Error) {
                console.warn("studio.qinda.screensaver: the "
                    + root.configuration.Saver
                    + " scene could not be loaded; showing the plain ground."
                    + " Is its package installed?")
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Compositor.Appearance 1.0
import QindaQt.Compositor.Containers 1.0

QtObject {
    readonly property var containers: ContainerIdentities.byWindow
    readonly property var palette: Appearance.qmlPalette
}

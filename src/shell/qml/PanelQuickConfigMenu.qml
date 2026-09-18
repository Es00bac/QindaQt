// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// Panel configuration menu (QQC2 style palette; Controls ships no menu
// primitive yet — task-list context menu precedent). Every quick setting
// persists through the PanelQuickConfig facade into Settings1.
// Plain right-click configuration menu of one panel surface (QQC2 style
// palette; Controls ships no menu primitive yet -- task-list context menu
// precedent). Every quick setting persists through the PanelQuickConfig
// facade into Settings1 (`panels.configuration`); PanelContent resolves the
// effective values and applies the emitted settings.
T.Menu {
    id: root

    // PanelQuickConfig facade and the effective quick settings PanelContent
    // resolves (schema defaults first, persisted values win).
    property var panelQuickConfig: null
    property bool dockMode: false
    property bool panelTransparency: true
    property bool dockZoom: true
    property int dockTileSize: 60

    signal applySetting(string key, var value)

    objectName: "panelConfigMenu"
    popupType: T.Popup.Window

    T.MenuItem {
        objectName: "panelConfigCustomize"
        text: qsTr("Customize Panel…")
        enabled: root.panelQuickConfig !== null
        onTriggered: root.panelQuickConfig.openCustomize()
    }
    T.MenuSeparator {}
    T.MenuItem {
        objectName: "panelConfigTransparency"
        text: qsTr("Transparency")
        checkable: true
        checked: root.panelTransparency
        enabled: Tokens.ready && !Boolean(Tokens.accessibility.reducedTransparency)
                 && !Boolean(Tokens.accessibility.highContrast)
        onTriggered: root.applySetting("transparency", checked)
    }
    T.MenuItem {
        objectName: "panelConfigDockZoom"
        visible: root.dockMode
        text: qsTr("Magnification")
        checkable: true
        checked: root.dockZoom
        enabled: Tokens.ready && !Boolean(Tokens.accessibility.reducedMotion)
        onTriggered: root.applySetting("dockZoom", checked)
    }
    T.Menu {
        objectName: "panelConfigTileSize"
        title: qsTr("Tile size (logical px)")
        visible: root.dockMode

        T.Slider {
            objectName: "panelConfigTileSizeSlider"
            width: 220; from: 32; to: 64; stepSize: 1
            snapMode: T.Slider.SnapAlways
            value: root.dockTileSize
            Accessible.name: qsTr("Dock tile size in logical pixels")
            onMoved: root.applySetting("dockTileSize", Math.round(value))
        }

        T.SpinBox {
            objectName: "panelConfigTileSizeInput"
            width: 220; from: 32; to: 64; stepSize: 1
            editable: true
            value: root.dockTileSize
            Accessible.name: qsTr("Dock tile size in logical pixels")
            Accessible.description: qsTr("From 32 to 64 logical pixels")
            onValueModified: root.applySetting("dockTileSize", value)
        }
    }
}


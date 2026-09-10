// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// One panel inside the WYSIWYG monitor. The material reproduces the desktop
// panel surface (dock vs bar coloring, corner radius by alignment,
// translucency, divider border) from live tokens, and applets render as icon
// chips in their real zones. Geometry is the repository-projected rectangle;
// the cross-axis thickness only gets a floor so chips stay clickable when
// the representative 1920-wide output is scaled down.
Item {
    id: root

    required property var panelData
    required property var customizeSettings
    required property real canvasScale

    readonly property bool horizontal:
        panelData.edge === "top" || panelData.edge === "bottom"
    readonly property real scaledThickness:
        (root.horizontal ? panelData.height : panelData.width) * canvasScale
    // Legibility floor shared with the desktop concept preview: 24px keeps
    // a 20px icon chip clickable inside the strip at representative scale.
    readonly property real visualThickness: Math.max(scaledThickness, Tokens.space["6"])
    readonly property bool selected:
        customizeSettings.selectedKind === "panel"
        && customizeSettings.selectedPanelId === panelData.id
    // Dock appearance derives from the same solved profile role the live
    // panel surface uses; it is presentation state, never schema.
    readonly property bool dockMode: horizontal && panelData.edge === "bottom"
        && panelData.alignment === "center"
        && (((panelData.applets ?? []).some(applet =>
                (applet.settings ?? {}).dockMode === true))
            || panelData.id === "dock" || panelData.id === "smart-shelf")

    objectName: "customizeCanvasPanel_" + (panelData.id ?? "")
    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("%1, panel").arg(panelData.name ?? "")
    Accessible.description: qsTr("Select to edit this panel")

    // Solver rectangles anchored at their edge; the thickness floor grows
    // toward the desktop interior so an edge-attached panel never detaches.
    width: horizontal ? panelData.width * canvasScale : visualThickness
    height: horizontal ? visualThickness : panelData.height * canvasScale
    x: panelData.x * canvasScale
       + (panelData.edge === "right"
          ? Math.max(0, panelData.width * canvasScale - width) : 0)
    y: panelData.y * canvasScale
       + (panelData.edge === "bottom"
          ? Math.max(0, panelData.height * canvasScale - height) : 0)

    function selectPanel() {
        root.customizeSettings.selectPanel(root.panelData.id ?? "")
    }

    TapHandler {
        onTapped: root.selectPanel()
    }

    Rectangle {
        id: material

        anchors.fill: parent
        radius: root.panelData.alignment === "fill" ? 0
                : root.dockMode ? Tokens.radius.l : Tokens.radius.m
        color: root.dockMode ? Tokens.bg.raised
               : Qt.lighter(Tokens.bg.base, 1.08)
        opacity: 0.96
        // The preview has no blur behind the material, so a strong hairline
        // keeps an edge-attached bar legible over the backdrop gradient the
        // way translucency does on the live desktop.
        border.width: root.customizeSettings.visualDragActive ? Tokens.space["1"]
                      : root.selected ? Tokens.space["1"]
                      : Tokens.space["1"] / 2
        border.color: root.customizeSettings.visualDragActive
                      ? (root.customizeSettings.dropAccepted
                         ? Tokens.status.success.foreground : Tokens.accent.default)
                      : root.selected ? Tokens.accent.default
                      : Tokens.outline.strong

        Behavior on border.color {
            ColorAnimation { duration: Tokens.motion.short }
        }
    }

    Item {
        id: zoneHost

        anchors.fill: parent
        // AGENT-GUARD: a drag rebuilds the panel delegates mid-gesture; while
        // an outgoing delegate is torn down its bindings re-evaluate and the
        // panel id reads null. All cross-references go through these guards
        // so reconstruction never emits warnings (the rows are fatal).
        readonly property bool dragActive:
            root !== null && root.customizeSettings.visualDragActive
        readonly property bool dropAccepted:
            root !== null && root.customizeSettings.dropAccepted

        Repeater {
            model: ["start", "center", "end"]

            delegate: Item {
                id: zone

                required property string modelData
                required property int index

                readonly property bool horiz: root !== null && root.horizontal
                readonly property var settingsRef:
                    root !== null ? root.customizeSettings : null
                readonly property var zoneApplets:
                    root !== null
                    ? (root.panelData.applets ?? []).filter(
                          applet => applet.zone === zone.modelData)
                    : []
                readonly property string targetPanelId:
                    root !== null ? (root.panelData.id ?? "") : ""
                readonly property string targetZone: zone.modelData

                objectName: "customizeDrop_" + targetPanelId + "_" + targetZone
                x: horiz && parent !== null ? index * parent.width / 3 : 0
                y: !horiz && parent !== null ? index * parent.height / 3 : 0
                width: parent !== null
                       ? (horiz ? parent.width / 3 : parent.width) : 0
                height: parent !== null
                        ? (horiz ? parent.height : parent.height / 3) : 0

                // Drop affordance: the zone under a live drag lights up so
                // the release point is unambiguous.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Tokens.space["1"] / 2
                    radius: Tokens.radius.s
                    visible: zoneHost.dragActive
                    color: zoneHost.dropAccepted ? Tokens.state.hover
                                                 : "transparent"
                    border.width: zoneHost.dragActive ? Tokens.space["1"] / 2 : 0
                    border.color: Tokens.outline.strong
                }

                Flow {
                    anchors.fill: parent
                    anchors.margins: Tokens.space["1"]
                    spacing: Tokens.space["1"]
                    clip: true

                    Repeater {
                        model: zone.zoneApplets

                        delegate: Rectangle {
                            id: chip

                            required property var modelData

                            readonly property int chipExtent: Math.max(
                                16, Math.min(26,
                                    (zone.horiz
                                     ? zone.height : zone.width) - 2 * Tokens.space["1"]))
                            readonly property bool chipSelected:
                                zone.settingsRef !== null
                                && zone.settingsRef.selectedKind === "applet"
                                && zone.settingsRef.selectedAppletId
                                   === chip.modelData.id

                            objectName: "customizeChip_" + (chip.modelData.id ?? "")
                            width: chipExtent
                            height: chipExtent
                            radius: Math.min(Tokens.radius.m, chipExtent / 3)
                            color: chipSelected ? Tokens.accent.subtle
                                   : hover.hovered ? Tokens.state.hover
                                   : Tokens.bg.raised
                            border.width: chipSelected || hover.hovered
                                          ? Tokens.space["1"] / 2 : 0
                            border.color: chipSelected ? Tokens.accent.default
                                          : Tokens.outline.strong
                            Accessible.role: Accessible.ListItem
                            Accessible.name: qsTr("%1 applet")
                                               .arg(chip.modelData.name ?? "")
                            Accessible.description: qsTr("Shown in the %1 zone")
                                                      .arg(zone.modelData)

                            readonly property string dragPluginId:
                                chip.modelData.pluginId ?? ""
                            readonly property string dragPanelId:
                                zone.targetPanelId
                            readonly property string dragAppletId: chip.modelData.id ?? ""

                            T.ToolTip.visible: hover.hovered
                            T.ToolTip.delay: 500
                            T.ToolTip.text: chip.modelData.name ?? ""

                            CustomizeAppletIcon {
                                anchors.centerIn: parent
                                pluginId: chip.modelData.pluginId ?? ""
                                iconSize: Math.max(12, chip.chipExtent
                                                   - 2 * Tokens.space["1"])
                            }

                            HoverHandler { id: hover }

                            TapHandler {
                                onTapped: zone.settingsRef !== null
                                          && zone.settingsRef.selectApplet(
                                              zone.targetPanelId,
                                              chip.modelData.id ?? "")
                            }
                        }
                    }
                }
            }
        }
    }
}

// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Window
import QindaQt.Tokens 1.0

// Shell icon element over the confined `image://qindaqt-icon/` provider.
// `name` is an XDG icon name; when it does not resolve (or no IconRuntime
// is installed on this engine) the element renders the typed placeholder:
// the first letter of `fallbackText` on a token-colored rounded tile. All
// colors come from QST-1 tokens; nothing is hard-coded.
Item {
    id: root

    property string name: ""
    // Logical pixel edge of the square icon; clamped to the module bounds.
    property int size: 24
    // Recolor target for symbolic SVGs. The default transparent value means
    // "no recolor"; it is never painted. Only fully opaque colors recolor:
    // the provider accepts only the opaque #rrggbb form, so a
    // semi-transparent color (serialized with an alpha channel) is not a
    // recolor target and the symbolic icon keeps its own pixels.
    property color color: "transparent"
    property bool symbolic: false
    // Accessible and placeholder text for the unresolved case.
    property string fallbackText: ""

    // AGENT-GUARD: `name` is bounded here as well as in the provider:
    // over-long or out-of-grammar names never reach the URL layer and behave
    // as unresolved. The grammar mirrors
    // IconThemeLocator::isAcceptableIconName (bounded ASCII, no `..`).
    readonly property string effectiveName: {
        const candidate = root.name
        if (candidate.length === 0 || candidate.length > 128
                || candidate.indexOf("..") >= 0) {
            return ""
        }
        return /^[A-Za-z0-9._-]+$/.test(candidate) ? candidate : ""
    }

    readonly property int effectiveSize: Math.max(1, Math.min(512, root.size))
    readonly property real effectiveScale: root.Window.window !== null
        ? Math.max(1, Math.min(4, root.Window.window.devicePixelRatio))
        : 1
    readonly property bool resolved: root.effectiveName.length > 0
        && IconLookup.hasIcon(root.effectiveName, root.effectiveSize,
                              root.effectiveScale, root.symbolic)

    implicitWidth: effectiveSize
    implicitHeight: effectiveSize

    Accessible.role: resolved ? Accessible.Graphic : Accessible.StaticText
    Accessible.name: root.fallbackText.length > 0 ? root.fallbackText : root.effectiveName

    Image {
        id: iconImage
        objectName: "iconImage"
        anchors.fill: parent
        visible: root.resolved
        source: root.resolved
            ? "image://qindaqt-icon/" + root.effectiveName
                + "?size=" + root.effectiveSize
                + "&scale=" + root.effectiveScale
                + (root.symbolic ? "&symbolic=1" : "")
                + (root.color.a > 0 ? "&color=" + encodeURIComponent(root.color.toString()) : "")
            : ""
        sourceSize.width: Math.round(root.effectiveSize * root.effectiveScale)
        sourceSize.height: Math.round(root.effectiveSize * root.effectiveScale)
        smooth: true
        fillMode: Image.PreserveAspectFit
    }

    Rectangle {
        id: placeholderTile
        objectName: "placeholderTile"
        anchors.fill: parent
        visible: !root.resolved
        radius: Tokens.radius.s
        color: Tokens.bg.highest
        border.width: 1
        border.color: Tokens.outline.divider

        Text {
            objectName: "placeholderGlyph"
            anchors.centerIn: parent
            text: root.fallbackText.length > 0
                ? root.fallbackText.charAt(0).toUpperCase()
                : "?"
            color: Tokens.fg.muted
            font.family: Tokens.type.fontFamily
            font.pointSize: Math.max(1, Tokens.type.body * 0.75)
        }
    }
}

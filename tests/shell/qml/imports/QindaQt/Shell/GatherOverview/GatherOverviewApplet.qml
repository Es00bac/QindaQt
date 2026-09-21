// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production applet is exercised by
// qindaqt.gather-overview-composition and the surface's own offscreen rows;
// this source module keeps the panel dispatcher tests independent of
// static-plugin registration in qmltestrunner.
//
// AGENT-CONTRACT: the property set here must match what
// BuiltinAppletContent.qml assigns. `access` is the gather overview
// composition and is NOT required: a session whose task-list grants were
// denied has none, and the applet draws its disabled state instead.
Item {
    property var access: null
    property bool vertical: false

    objectName: "gatherOverviewApplet"
    implicitWidth: 28
    implicitHeight: 28
}

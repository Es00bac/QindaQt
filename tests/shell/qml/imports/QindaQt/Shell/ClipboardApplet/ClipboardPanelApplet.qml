// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production module has an independent
// popup keyboard test; this fixture verifies shared panel inventory and the
// purpose-specific controller propagation without importing service code.
Item {
    required property var controller
    required property var theme
    property bool vertical: false

    objectName: "clipboardPanelApplet"
    implicitWidth: 64
    implicitHeight: 28
    enabled: controller !== null
}

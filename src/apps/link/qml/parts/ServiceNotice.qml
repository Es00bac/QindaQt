// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.Link

// Shown on every page when the QindaLink service is not on the bus, so an empty page is never
// mistaken for an empty phone.
Tk.Notice {
    visible: !Devices.serviceAvailable
    variant: "warning"
    title: qsTr("QindaLink service is not running")
    text: qsTr("Start it with: systemctl --user start qindaqt-link-service")
}

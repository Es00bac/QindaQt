// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// One device picker for a console strip or bus (ADR-0178): "Automatic" first,
// then every device of the right kind. The current entry is the device the
// element is bound to right now, and a pin is marked so the user can tell
// "I chose this" from "the service chose this".
Tk.ComboBox {
    id: picker

    // Rows shaped like the model's inputDevices / outputDevices.
    required property var devices
    // The serial the element is bound to, 0 when unbound.
    required property int boundSerial
    required property bool pinned
    property string accessibleName

    signal picked(int serial)

    readonly property var entries: {
        const rows = [{ serial: 0, label: qsTr("Automatic") }]
        for (const device of picker.devices) {
            rows.push({ serial: Number(device.serial),
                        label: picker.pinned && Number(device.serial) === picker.boundSerial
                            ? qsTr("%1 (pinned)").arg(device.displayName)
                            : device.displayName })
        }
        return rows
    }

    small: true
    model: entries
    textRole: "label"
    // AGENT-GUARD: the index is derived from the bound serial, never stored,
    // so a device appearing or disappearing cannot leave the picker pointing at
    // the wrong row. An unbound pinned strip shows "Automatic" is NOT selected
    // only through the pinned marker on its device row when that device is
    // back; while absent, nothing in the list is it, and index 0 is honest.
    currentIndex: {
        if (!picker.pinned && picker.boundSerial === 0) {
            return 0
        }
        for (let index = 1; index < entries.length; ++index) {
            if (entries[index].serial === picker.boundSerial) {
                return index
            }
        }
        return 0
    }
    onActivated: index => picker.picked(entries[index].serial)
    Accessible.name: accessibleName
    tooltip: accessibleName
}

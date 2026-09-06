// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Lint-only description of the KWin-created runtime type. Production resolves
// the type from KWin's process-local QML registration, which has no installed
// qmltypes file for standalone tooling.
Item {
    property int currentIndex: 0
    property var model: null
    property rect screenGeometry: Qt.rect(0, 0, 1920, 1080)
}

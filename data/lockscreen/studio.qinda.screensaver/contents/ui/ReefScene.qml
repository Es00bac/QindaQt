// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.CircuitReef

// privateMetrics keeps the scene from drawing live system counters on a
// locked screen, the same contract the standalone saver's --private carries.
CircuitReef {
    active: true
    privateMetrics: true
}

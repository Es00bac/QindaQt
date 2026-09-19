// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Qinda.Patrol

// Telemetry stays off: a locked screen must not render live CPU, memory or
// network counters to whoever is standing in front of it.
PatrolScene {
    running: true
    metricsEnabled: false
}

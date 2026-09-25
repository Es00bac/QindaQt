// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "kwinoutputinventory.h"

#include <QString>
#include <QVector>

namespace KWin {
class BackendOutput;
}

namespace QindaQt::Compositor::KWinIntegration {

// Samplers for the mode and mirror members of one OutputInventoryEntry. They
// borrow KWin outputs for the call only, run on KWin's GUI thread, and return
// values already inside OutputInventoryStore's bounds.

// Advertised modes in KWin order, one per size+refresh, at most
// OutputInventoryStore::MaxModes, and always including the current mode.
[[nodiscard]] QVector<OutputInventoryMode> sampleAdvertisedModes(
    const KWin::BackendOutput &output);

// Connector name of the enabled, non-mirroring output in `published` that
// `output` replicates, or empty. KWin names the source by runtime UUID.
[[nodiscard]] QString sampleReplicationSource(
    const KWin::BackendOutput &output,
    const QVector<const KWin::BackendOutput *> &published);

} // namespace QindaQt::Compositor::KWinIntegration

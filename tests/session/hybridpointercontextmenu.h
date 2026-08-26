// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositorprobeclient.h"
#include "hybridpointerinventory.h"

#include <QPointF>

#include <optional>

namespace QindaQt::Test {

class HybridPointerGrouping;
struct HybridPointerGroupedState;

struct HybridContextMenuEvidence final
{
    WindowInventory keptAbove;
    WindowInventory restored;
    HybridDiagnostics diagnostics;
    QPointF sharedTitlePoint;
};

// Drives the production outer-title router and QMenu. Success requires the
// one-representative KWin mutation to settle on every group member, which can
// only happen through KWinGroupContextManager's queued adoption transaction.
[[nodiscard]] std::optional<HybridContextMenuEvidence>
exerciseHybridContextMenu(CompositorProbeClient &client,
                          HybridPointerGrouping &pointer,
                          const HybridPointerGroupedState &state,
                          const WindowInventory &raised,
                          const QPointF &sharedTitlePoint,
                          QString *error);

} // namespace QindaQt::Test

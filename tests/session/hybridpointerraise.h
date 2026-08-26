// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositordevelopmentworkflow.h"
#include "compositorprobeclient.h"
#include "hybridpointerinventory.h"

#include <QPointF>
#include <QStringList>

#include <functional>
#include <optional>

namespace QindaQt::Test {

class HybridPointerGrouping;
struct HybridPointerGroupedState;

struct RaisedGroupEvidence final
{
    WindowInventory covered;
    WindowInventory raised;
    HybridDiagnostics diagnostics;
    QPointF coveredSharedTitlePoint;
    QPointF sharedTitlePoint;
};

// Covers shared chrome with an unrelated window, proves covered and popup-grab
// presses do not tunnel, then raises the exposed group through ordinary input.
[[nodiscard]] std::optional<RaisedGroupEvidence> coverAndRaiseGroup(
    CompositorProbeClient &client,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    const QStringList &probeTitles,
    const std::function<void(const QString &)> &activateProbe,
    const std::function<void(const QString &)> &showPopupForProbe,
    QString *error);

} // namespace QindaQt::Test

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QtTypes>

#include <optional>

class QWindow;

namespace QindaQt::Test {

class CompositorProbeClient;
class HybridPointerGrouping;
struct HybridPointerGroupedState;

struct HybridTaskIdentityLifecycleEvidence final
{
    quint64 regroupedRevision = 0;
    quint64 inactivePageRevision = 0;
    quint64 reactivatedSplitRevision = 0;
    QString activeSplitPageId;
};

// Exercises page creation, hostile inactive-page exposure, page switch-back,
// native whole-group minimize, and active-page-only restore while the caller's
// HybridPointerGrouping keeps the selected input device alive.
[[nodiscard]] std::optional<HybridTaskIdentityLifecycleEvidence>
exerciseHybridTaskIdentityLifecycle(
    QWindow &primary,
    QWindow &secondary,
    QWindow &page,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    CompositorProbeClient &client,
    QString *error);

} // namespace QindaQt::Test

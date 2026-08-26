// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

namespace QindaQt::Test {

class CompositorProbeClient;
struct HybridPointerGroupedState;

struct HybridCompositorRestartEvidence final
{
    QString topologyRevision;
    QString containerRevision;
};

// Drives KWin's public compositor-reinitialization signal while a real Hybrid
// group exists, then proves both client realization and scene chrome return.
[[nodiscard]] std::optional<HybridCompositorRestartEvidence>
exerciseHybridCompositorRestart(CompositorProbeClient &client,
                                const HybridPointerGroupedState &state,
                                QString *error);

} // namespace QindaQt::Test

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "touchedgeactions.h"

namespace QindaQt::Compositor::KWinIntegration {

// Reserves touch edges on KWin's ScreenEdges for the plugin's actions.
class KWinTouchEdgeReserver final : public TouchEdgeReserver {
public:
    [[nodiscard]] static bool available();
    void reserve(TouchEdge edge, QAction *action) override;
    void unreserve(TouchEdge edge, QAction *action) override;
};

} // namespace QindaQt::Compositor::KWinIntegration

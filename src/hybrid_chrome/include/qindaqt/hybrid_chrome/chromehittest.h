// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chrometypes.h"

namespace QindaQt::HybridChrome {

class ChromeHitTester final
{
public:
    // Coordinates are logical desktop coordinates in the plan's coordinate
    // system. Callers convert device pixels before crossing this boundary.
    [[nodiscard]] static ChromeHitTarget hitTest(const ChromeRenderPlan &plan,
                                                 const QPointF &logicalPosition);
};

} // namespace QindaQt::HybridChrome

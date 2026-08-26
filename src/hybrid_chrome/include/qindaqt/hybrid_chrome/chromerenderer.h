// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chrometypes.h"

class QPainter;

namespace QindaQt::HybridChrome {

class ChromeRenderer final
{
public:
    // The caller owns an active painter. Rendering reads only the plan/state
    // and never mutates compositor or client-window state.
    static void paint(QPainter &painter,
                      const ChromeRenderPlan &plan,
                      const ChromePaintState &state = {});
};

} // namespace QindaQt::HybridChrome

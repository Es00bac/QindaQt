// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "pointercorner.h"

namespace QindaQt::Compositor::KWinIntegration {

// Reserves the upper-left corner on KWin's ScreenEdges for the pointer.
class KWinPointerCornerReserver final : public PointerCornerReserver {
public:
    [[nodiscard]] static bool available();
    void reserve(QObject *object, const char *callback) override;
    void unreserve(QObject *object) override;
};

} // namespace QindaQt::Compositor::KWinIntegration

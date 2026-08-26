// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridwindowadmission.h"

namespace QindaQt::Compositor::KWinIntegration {

bool admitsHybridTopologyWindow(const HybridWindowAdmission &window) noexcept
{
    return window.exists && !window.deleted && !window.internal
        && !window.popup && window.normal && !window.transient
        && !window.dialog;
}

} // namespace QindaQt::Compositor::KWinIntegration

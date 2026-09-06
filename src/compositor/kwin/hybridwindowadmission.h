// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Compositor::KWinIntegration {

// Platform-value projection used at the ManagedWindowRegistry boundary. KWin
// window type and transient relationship are independent, so both dimensions
// must be represented explicitly.
struct HybridWindowAdmission final
{
    bool exists = false;
    bool deleted = false;
    bool internal = false;
    // Desktop-shell surfaces may report a normal window type, but their
    // placement remains owned by the layer-shell protocol rather than Hybrid.
    bool layerShell = false;
    bool popup = false;
    bool normal = false;
    bool transient = false;
    bool dialog = false;
};

// Only independent normal clients may become Hybrid topology leaves.
[[nodiscard]] bool admitsHybridTopologyWindow(
    const HybridWindowAdmission &window) noexcept;

} // namespace QindaQt::Compositor::KWinIntegration

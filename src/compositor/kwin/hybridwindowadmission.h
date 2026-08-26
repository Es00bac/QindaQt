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
    bool popup = false;
    bool normal = false;
    bool transient = false;
    bool dialog = false;
};

// Only independent normal clients may become Hybrid topology leaves.
[[nodiscard]] bool admitsHybridTopologyWindow(
    const HybridWindowAdmission &window) noexcept;

} // namespace QindaQt::Compositor::KWinIntegration

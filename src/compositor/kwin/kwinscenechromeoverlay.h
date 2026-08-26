// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

namespace QindaQt::Compositor::KWinIntegration {

class ChromeOverlayFactory;
class ManagedWindowRegistry;

// Creates the pinned-KWin scene adapter used by KWinChromeManager. The
// registry is borrowed and must outlive the factory and every overlay it owns.
[[nodiscard]] std::unique_ptr<ChromeOverlayFactory>
createKWinSceneChromeOverlayFactory(ManagedWindowRegistry &registry);

} // namespace QindaQt::Compositor::KWinIntegration

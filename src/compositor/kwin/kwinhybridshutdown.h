// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridshutdownrecovery.h"

namespace QindaQt::Compositor::KWinIntegration {

class HybridInteractionRuntime;
class KWinHybridSceneFactory;
class ManagedWindowRegistry;

// Adapts the production Hybrid collaborators to the toolkit-neutral bounded
// shutdown coordinator. Borrowed collaborators must remain alive for the
// complete synchronous call.
[[nodiscard]] HybridShutdownRecoveryResult recoverKWinHybridShutdown(
    HybridInteractionRuntime &runtime,
    KWinHybridSceneFactory &sceneFactory,
    ManagedWindowRegistry &registry);

} // namespace QindaQt::Compositor::KWinIntegration

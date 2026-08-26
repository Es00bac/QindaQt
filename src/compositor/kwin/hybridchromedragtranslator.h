// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid/windowtopology.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_input/interactiontargetresolver.h"
#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QString>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// Bridges value-only scene-router chrome events into the same interaction
// intent vocabulary used by modifier-pointer and keyboard docking. It owns no
// gesture state and retains no topology references.
class HybridChromeDragTranslator final
{
public:
    explicit HybridChromeDragTranslator(
        const HybridInput::InteractionTargetResolver &targetResolver);

    // Outer-resize and button events deliberately fail here; placement policy
    // handles resize edges directly, while this boundary covers only intent
    // kinds understood by HybridInteractionRuntime.
    [[nodiscard]] std::optional<HybridInput::InteractionIntent> translate(
        const Hybrid::WindowTopology &topology,
        const QString &containerId,
        const HybridChrome::ChromeDragEvent &event,
        QString *error = nullptr) const;

private:
    const HybridInput::InteractionTargetResolver &m_targetResolver;
};

} // namespace QindaQt::Compositor::KWinIntegration

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/sceneadapter.h"

#include <QHash>
#include <QRectF>

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

class KWinSceneAdapter final : public SceneAdapter
{
public:
    explicit KWinSceneAdapter(ManagedWindowRegistry &registry);

    [[nodiscard]] std::unique_ptr<SceneTransaction> prepareTransition(
        const Core::WindowContainer &before,
        const Core::WindowContainer &after,
        QString *error = nullptr) override;

private:
    [[nodiscard]] QRectF outerFrame(const Core::WindowContainer &before,
                                    const Core::WindowContainer &after) const;

    ManagedWindowRegistry &m_registry;
    QHash<QString, QRectF> m_restoreFrames;
};

} // namespace QindaQt::Compositor::KWinIntegration

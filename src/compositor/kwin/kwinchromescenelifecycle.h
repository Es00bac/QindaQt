// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

#include <functional>

namespace QindaQt::Compositor::KWinIntegration {

// Serializes the scene-resource half of a KWin compositor restart. The
// compositor signal adapter invokes prepareForSceneTeardown() synchronously;
// the callbacks are borrowed by value and run only on KWin's GUI thread.
class KWinChromeSceneLifecycle final : public QObject
{
public:
    using ReleaseSceneResources = std::function<void()>;
    using RepublishSceneResources = std::function<void()>;

    KWinChromeSceneLifecycle(ReleaseSceneResources release,
                             RepublishSceneResources republish,
                             bool sceneAvailable = true,
                             QObject *parent = nullptr);

    // AGENT-CONTRACT: KWinHybridSession connects aboutToToggleCompositing and
    // aboutToDestroy with Qt::DirectConnection. Returning from this method
    // guarantees no QindaQt ImageItem remains parented to a WindowItem.
    void prepareForSceneTeardown() noexcept;
    // KWin emits active=true only after the new scene and all WindowItems exist.
    void compositingToggled(bool active);

    [[nodiscard]] bool sceneAvailable() const noexcept
    {
        return m_sceneAvailable;
    }

private:
    ReleaseSceneResources m_release;
    RepublishSceneResources m_republish;
    bool m_sceneAvailable = true;
};

} // namespace QindaQt::Compositor::KWinIntegration

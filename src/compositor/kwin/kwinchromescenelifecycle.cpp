// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinchromescenelifecycle.h"

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

KWinChromeSceneLifecycle::KWinChromeSceneLifecycle(
    ReleaseSceneResources release,
    RepublishSceneResources republish,
    bool sceneAvailable,
    QObject *parent)
    : QObject(parent)
    , m_release(std::move(release))
    , m_republish(std::move(republish))
    , m_sceneAvailable(sceneAvailable)
{
}

void KWinChromeSceneLifecycle::prepareForSceneTeardown() noexcept
{
    if (!m_sceneAvailable) {
        return;
    }
    // AGENT-GUARD: Mark unavailable before callbacks emit visibility or stack
    // signals. Those synchronous signals can request chrome synchronization;
    // it must observe the suspended state and never recreate an ImageItem in
    // the scene KWin is currently dismantling.
    m_sceneAvailable = false;
    if (m_release) {
        m_release();
    }
}

void KWinChromeSceneLifecycle::compositingToggled(bool active)
{
    if (!active || m_sceneAvailable) {
        return;
    }
    // KWin 6.6.5 emits compositingToggled(true) after setupCompositing() has
    // recreated every WindowItem. Make that fact visible to synchronizeChrome
    // before asking it to anchor fresh scene items.
    m_sceneAvailable = true;
    if (m_republish) {
        m_republish();
    }
}

} // namespace QindaQt::Compositor::KWinIntegration

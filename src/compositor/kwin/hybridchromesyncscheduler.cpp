// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromesyncscheduler.h"

#include <QTimer>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

HybridChromeSyncScheduler::HybridChromeSyncScheduler(Callback callback,
                                                     QObject *parent)
    : QObject(parent)
    , m_callback(std::move(callback))
{
}

void HybridChromeSyncScheduler::stackingOrderChanged()
{
    request(HybridChromeSyncReason::Stacking);
}

void HybridChromeSyncScheduler::activeWindowChanged()
{
    request(HybridChromeSyncReason::Activation);
}

void HybridChromeSyncScheduler::outputsChanged()
{
    request(HybridChromeSyncReason::Outputs);
}

void HybridChromeSyncScheduler::windowsChanged()
{
    request(HybridChromeSyncReason::Windows);
}

void HybridChromeSyncScheduler::request(HybridChromeSyncReason reason)
{
    // Compacting a grouped member block can synchronously produce another KWin
    // stacking signal. The in-flight pass already samples the resulting
    // workspace state, so queuing that callback would create an endless loop.
    if (m_synchronizing) {
        return;
    }
    m_reasons |= reason;
    if (m_pending) {
        return;
    }
    m_pending = true;
    QTimer::singleShot(0, this, [this] { synchronize(); });
}

void HybridChromeSyncScheduler::synchronize()
{
    m_pending = false;
    const auto reasons = m_reasons;
    m_reasons = {};
    if (!m_callback || !reasons) {
        return;
    }
    m_synchronizing = true;
    m_callback(reasons);
    m_synchronizing = false;
}

} // namespace QindaQt::Compositor::KWinIntegration

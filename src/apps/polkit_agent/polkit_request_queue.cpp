// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_request_queue.h"

#include <utility>

namespace QindaQt::Apps::PolkitAgent {

PolkitRequestQueue::PolkitRequestQueue(ControllerFactory factory, QObject *parent)
    : QObject(parent)
    , m_factory(std::move(factory))
{
}

void PolkitRequestQueue::enqueue(AuthenticationRequest request)
{
    m_pending.push_back(std::move(request));
    if (!m_active) {
        activateNext();
    }
}

void PolkitRequestQueue::activateNext()
{
    if (m_pending.empty()) {
        return;
    }
    m_activeRequest = std::move(m_pending.front());
    m_pending.pop_front();
    m_active = m_factory(m_activeRequest);
    // AGENT-GUARD: connect before start() so a synchronous first-attempt
    // failure (an empty identity list, say) still reaches this lambda rather
    // than leaving the queue permanently stuck on a controller that can
    // never complete.
    connect(m_active.get(), &PolkitAttemptController::completed, this, [this](bool) {
        m_active.reset();
        if (m_pending.empty()) {
            Q_EMIT queueIdle();
        } else {
            activateNext();
        }
    });
    Q_EMIT requestActivated(m_activeRequest, m_active.get());
    m_active->start();
}

void PolkitRequestQueue::cancelActive()
{
    if (m_active) {
        m_active->cancelExternally();
    }
}

} // namespace QindaQt::Apps::PolkitAgent

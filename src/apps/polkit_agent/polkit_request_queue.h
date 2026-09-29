// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agent_identity.h"
#include "polkit_attempt_controller.h"
#include "polkit_requester_resolver.h"

#include <QList>
#include <QObject>
#include <QString>

#include <deque>
#include <functional>
#include <memory>

namespace QindaQt::Apps::PolkitAgent {

// Everything the dialog needs to know about one initiateAuthentication call,
// with every polkit type already translated to a pure value on the way in.
struct AuthenticationRequest final {
    QString actionId;
    QString message;
    QString iconName;
    QString vendorName;
    QString cookie;
    QList<AgentIdentity> identities;
    RequesterInfo requester;
};

using ControllerFactory =
    std::function<std::unique_ptr<PolkitAttemptController>(const AuthenticationRequest &)>;

// One authentication dialog is ever open at a time. A second
// initiateAuthentication that arrives while one is open is queued, not
// dropped or rejected, and activates the moment the active one completes.
class PolkitRequestQueue final : public QObject {
    Q_OBJECT

public:
    explicit PolkitRequestQueue(ControllerFactory factory, QObject *parent = nullptr);

    void enqueue(AuthenticationRequest request);
    // polkitd cancelling "the" open authentication; see the AGENT-NOTE on
    // PolkitAttemptController::cancelExternally() for why this takes no
    // cookie.
    void cancelActive();

    [[nodiscard]] bool hasActive() const noexcept { return m_active != nullptr; }
    [[nodiscard]] qsizetype pendingCount() const noexcept
    {
        return static_cast<qsizetype>(m_pending.size());
    }
    [[nodiscard]] PolkitAttemptController *activeController() const noexcept
    {
        return m_active.get();
    }
    [[nodiscard]] const AuthenticationRequest *activeRequest() const noexcept
    {
        return m_active ? &m_activeRequest : nullptr;
    }

Q_SIGNALS:
    // Emitted once a request becomes the active one, whether immediately
    // (the queue was idle) or later (its turn came up).
    void requestActivated(const AuthenticationRequest &request,
                          PolkitAttemptController *controller);
    void queueIdle();

private:
    void activateNext();

    ControllerFactory m_factory;
    std::unique_ptr<PolkitAttemptController> m_active;
    AuthenticationRequest m_activeRequest;
    std::deque<AuthenticationRequest> m_pending;
};

} // namespace QindaQt::Apps::PolkitAgent

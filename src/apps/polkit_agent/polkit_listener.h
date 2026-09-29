// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "polkit_request_queue.h"
#include "polkit_requester_resolver.h"

#include <PolkitQt1/Agent/Listener>
#include <PolkitQt1/Details>
#include <PolkitQt1/Identity>

#include <QHash>
#include <QObject>
#include <QString>

namespace QindaQt::Apps::PolkitAgent {

// The one file in this module that includes a PolkitQt1 header. Registers
// with polkitd as the session's sole authentication agent (ADR-0290) and
// translates every polkit type crossing this boundary into the pure
// AuthenticationRequest/AgentIdentity/AuthenticationAttempt shapes the rest
// of the module is tested against.
class PolkitListener final : public PolkitQt1::Agent::Listener {
    Q_OBJECT

public:
    explicit PolkitListener(PolkitRequesterResolver &requesterResolver,
                                 QObject *parent = nullptr);
    ~PolkitListener() override;

    [[nodiscard]] PolkitRequestQueue &queue() noexcept { return m_queue; }

    void initiateAuthentication(const QString &actionId, const QString &message,
                                const QString &iconName, const PolkitQt1::Details &details,
                                const QString &cookie,
                                const PolkitQt1::Identity::List &identities,
                                PolkitQt1::Agent::AsyncResult *result) override;
    bool initiateAuthenticationFinish() override;
    void cancelAuthentication() override;

private:
    PolkitRequesterResolver &m_requesterResolver;
    // AGENT-CONTRACT: keyed by polkit's cookie, which is unique per open
    // request; entries are inserted in initiateAuthentication() and taken
    // (removed) exactly once, the moment that request's controller reports
    // completed(), so result->setCompleted() fires exactly once per cookie
    // regardless of how many retries happened in between.
    QHash<QString, PolkitQt1::Agent::AsyncResult *> m_pendingResults;
    PolkitRequestQueue m_queue;
};

} // namespace QindaQt::Apps::PolkitAgent

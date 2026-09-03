// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>

#include <qindaqt/services/network_protocol/network_redaction.h>

#include <utility>

namespace QindaQt::Network::Client {

bool NetworkClient::operationAdmissionReady() const noexcept {
    return m_started && !m_owner.isEmpty() && m_state == ClientState::Ready
           && m_model.snapshot().has_value() && !m_request && !m_operation
           && !m_refreshTimer.isActive() && m_nextToken != 0;
}

bool NetworkClient::beginOperation(const OperationKind kind,
                                   const QVariantMap &parameters,
                                   QString *error) {
    if (!operationAdmissionReady()) {
        QString reason = QStringLiteral("client-not-ready");
        if (m_nextToken == 0) {
            reason = QStringLiteral("network request token is exhausted");
        } else if (m_operation
                   || (m_request
                       && m_request->kind == RequestKind::Operation)) {
            reason = QStringLiteral("operation-in-flight");
        }
        setError(error, std::move(reason));
        return false;
    }
    if (wireContainsSecrets(parameters)) {
        // AGENT-GUARD: This remains a second structural fence after model
        // admission; no consumer bug may put a credential on Network1.
        setError(error, QStringLiteral("operation-parameters-contain-secrets"));
        return false;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        setError(error, QStringLiteral("network request token is exhausted"));
        return false;
    }
    const auto lineage = m_model.lineage();
    m_operation = Operation{kind, lineage->epoch, lineage->revision};
    m_request = Request{token, m_owner, RequestKind::Operation, lineage->epoch,
                        lineage->revision, kind};
    m_timeout.start(m_timing.requestTimeoutMilliseconds);
    Q_EMIT operationInFlightChanged();
    notifyOperationAdmissionChanged();
    m_transport.requestOperation(token, m_owner, lineage->epoch,
                                 lineage->revision, kind, parameters);
    return true;
}

bool NetworkClient::connectVisibleNetwork(const QString &accessPointId,
                                          QString *error) {
    const Model::IntentVerdict verdict =
        m_model.connectVisible(ConnectVisibleIntent{accessPointId});
    if (!verdict.allowed) {
        setError(error, verdict.reasonCode);
        return false;
    }
    QVariantMap parameters;
    parameters.insert(QStringLiteral("accessPointId"), accessPointId);
    return beginOperation(OperationKind::ConnectVisibleNetwork, parameters,
                          error);
}

void NetworkClient::notifyOperationAdmissionChanged() {
    const bool ready = operationAdmissionReady();
    if (ready == m_lastOperationAdmissionReady) {
        return;
    }
    m_lastOperationAdmissionReady = ready;
    Q_EMIT operationAdmissionChanged();
}

} // namespace QindaQt::Network::Client

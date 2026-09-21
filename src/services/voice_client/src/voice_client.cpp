// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/voice_client/voice_client.h>

#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <limits>

namespace QindaQt::Services::Voice {

VoiceClient::VoiceClient(VoiceTransport *transport, QObject *parent)
    : QObject(parent), m_transport(transport)
{
    Q_ASSERT(m_transport != nullptr);
    m_fetchTimer.setSingleShot(true);
    m_operationTimer.setSingleShot(true);
    connect(&m_fetchTimer, &QTimer::timeout, this, [this] {
        m_fetching = false;
        m_snapshot.reset();
        publishLevel(0);
        completeUncertain(QStringLiteral("snapshot-timeout"));
        publishState(ClientState::Unavailable, QStringLiteral("snapshot-timeout"));
    });
    connect(&m_operationTimer, &QTimer::timeout, this, [this] {
        completeUncertain(QStringLiteral("operation-timeout"));
        fetch();
    });
    connect(m_transport, &VoiceTransport::ownerChanged, this, &VoiceClient::acceptOwner);
    connect(m_transport, &VoiceTransport::invalidated, this,
            &VoiceClient::acceptInvalidation);
    connect(m_transport, &VoiceTransport::levelReported, this, &VoiceClient::acceptLevel);
    connect(m_transport, &VoiceTransport::snapshotReply, this,
            &VoiceClient::acceptSnapshot);
    connect(m_transport, &VoiceTransport::operationReply, this,
            &VoiceClient::acceptOperation);
}

void VoiceClient::start()
{
    if (m_state != ClientState::Stopped) {
        return;
    }
    publishState(ClientState::Starting, QStringLiteral("discovering-owner"));
    m_transport->start();
}

void VoiceClient::stop()
{
    if (m_state == ClientState::Stopped) {
        return;
    }
    completeUncertain(QStringLiteral("client-stopped"));
    m_fetchTimer.stop();
    m_operationTimer.stop();
    m_fetching = false;
    m_snapshot.reset();
    m_owner.clear();
    m_ownerResolved = false;
    publishLevel(0);
    m_transport->stop();
    publishState(ClientState::Stopped, {});
}

void VoiceClient::setRequestTimeout(const int milliseconds)
{
    m_timeoutMs = qBound(10, milliseconds, 60'000);
}

void VoiceClient::publishState(const ClientState state, const QString &reasonCode)
{
    if (m_state == state && m_reasonCode == reasonCode) {
        return;
    }
    m_state = state;
    m_reasonCode = reasonCode;
    Q_EMIT stateChanged(state, reasonCode);
}

void VoiceClient::publishLevel(const quint32 levelPercent)
{
    if (m_levelPercent == levelPercent) {
        return;
    }
    m_levelPercent = levelPercent;
    Q_EMIT levelChanged(levelPercent);
}

void VoiceClient::acceptOwner(const QString &owner)
{
    if (m_state == ClientState::Stopped) {
        return;
    }
    // AGENT-GUARD: the first empty owner must not be mistaken for "nothing
    // changed". Without a provider installed there is no second report, so
    // collapsing empty->empty leaves the panel saying "Voice…" forever
    // instead of "no voice provider is running".
    if (owner == m_owner && m_ownerResolved) {
        return;
    }
    m_ownerResolved = true;
    completeUncertain(QStringLiteral("owner-replaced"));
    m_fetchTimer.stop();
    m_fetching = false;
    m_snapshot.reset();
    m_owner = owner;
    publishLevel(0);
    if (owner.isEmpty()) {
        publishState(ClientState::Unavailable, QStringLiteral("service-unavailable"));
    } else {
        publishState(ClientState::Starting, QStringLiteral("fetching-snapshot"));
        fetch();
    }
}

void VoiceClient::fetch()
{
    if (m_owner.isEmpty() || m_state == ClientState::Stopped) {
        return;
    }
    if (m_fetching) {
        m_dirty = true;
        return;
    }
    if (m_nextToken == 0 || m_nextToken == std::numeric_limits<quint64>::max()) {
        publishState(ClientState::Unavailable, QStringLiteral("request-id-exhausted"));
        return;
    }
    m_fetching = true;
    m_dirty = false;
    m_fetchToken = m_nextToken++;
    m_fetchTimer.start(m_timeoutMs);
    m_transport->fetchSnapshot(m_owner, m_fetchToken);
}

void VoiceClient::acceptInvalidation(const QString &owner, const quint64 revision)
{
    if (owner != m_owner || owner.isEmpty()) {
        return;
    }
    // A revision we already hold needs no round trip; anything else, including
    // a regression, is a lineage question only a fresh snapshot can settle.
    if (!m_snapshot.has_value() || revision != m_snapshot->revision) {
        fetch();
    }
}

void VoiceClient::acceptLevel(const QString &owner, const quint32 levelPercent)
{
    if (owner != m_owner || owner.isEmpty() || !m_snapshot.has_value()) {
        return;
    }
    // AGENT-GUARD: a meter that keeps moving after capture ended reads as a hot
    // microphone. Level is only honoured while the snapshot says we capture.
    const bool capturing = m_snapshot->state == SessionState::Arming
                           || m_snapshot->state == SessionState::Listening;
    publishLevel(capturing ? clampLevelPercent(levelPercent) : 0);
}

void VoiceClient::acceptSnapshot(const QString &owner, const quint64 token,
                                 const bool transportSuccess, const Snapshot &snapshot,
                                 const QString &reasonCode)
{
    if (!m_fetching || owner != m_owner || token != m_fetchToken) {
        return;
    }
    m_fetchTimer.stop();
    m_fetching = false;
    const ValidationResult validation = validateSnapshot(snapshot);
    const bool duplicate = m_snapshot.has_value() && snapshot == *m_snapshot;
    // Within one owner a revision never moves backwards, and a revision that
    // repeats must carry identical values. Either contradiction means the
    // provider is no longer a single ordered authority.
    const bool contradiction =
        m_snapshot.has_value()
        && (snapshot.revision < m_snapshot->revision
            || (snapshot.revision == m_snapshot->revision && !duplicate));
    if (!transportSuccess || !validation.accepted || contradiction) {
        m_snapshot.reset();
        publishLevel(0);
        completeUncertain(QStringLiteral("snapshot-unavailable"));
        publishState(ClientState::Unavailable,
                     transportSuccess ? validation.accepted
                                            ? QStringLiteral("revision-contradiction")
                                            : validation.reasonCode
                                      : reasonCode);
        return;
    }
    m_snapshot = snapshot;
    publishState(ClientState::Ready, QStringLiteral("ready"));
    if (!duplicate) {
        Q_EMIT snapshotChanged(snapshot);
    }
    const bool capturing = snapshot.state == SessionState::Arming
                           || snapshot.state == SessionState::Listening;
    if (!capturing) {
        publishLevel(0);
    }
    if (m_dirty) {
        fetch();
    }
}

void VoiceClient::acceptOperation(const QString &owner, const quint64 token,
                                  const bool transportSuccess,
                                  const OperationResult &result, const QString &reasonCode)
{
    if (!m_operation.has_value() || owner != m_owner || token != m_operation->token) {
        return;
    }
    const Pending pending = *m_operation;
    m_operation.reset();
    m_operationTimer.stop();
    const ValidationResult validation = validateOperationResult(result);
    const bool exact = result.requestId == pending.request.requestId
        && result.kind == pending.request.kind
        && result.initiatingRevision == pending.request.expectedRevision;
    if (!transportSuccess || !validation.accepted || !exact) {
        complete(localResult(pending.request.kind, pending.request.requestId,
                             OperationStatus::Uncertain,
                             transportSuccess ? QStringLiteral("malformed-result")
                                              : reasonCode));
    } else {
        complete(result);
    }
    fetch();
}

} // namespace QindaQt::Services::Voice

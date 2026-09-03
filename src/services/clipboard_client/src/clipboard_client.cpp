// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_client/clipboard_client.h>

#include <qindaqt/services/clipboard_protocol/clipboard_validation.h>

#include <QtCore/QMetaObject>

#include <limits>

namespace QindaQt::Services::Clipboard {

ClipboardClient::ClipboardClient(ClipboardTransport *transport, QObject *parent)
    : QObject(parent), m_transport(transport)
{
    Q_ASSERT(m_transport != nullptr);
    m_fetchTimer.setSingleShot(true);
    m_operationTimer.setSingleShot(true);
    connect(&m_fetchTimer, &QTimer::timeout, this, [this] {
        m_fetching = false;
        m_snapshot.reset();
        completeUncertain(QStringLiteral("snapshot-timeout"));
        publishState(ClientState::Unavailable, QStringLiteral("snapshot-timeout"));
    });
    connect(&m_operationTimer, &QTimer::timeout, this, [this] {
        completeUncertain(QStringLiteral("operation-timeout"));
        fetch();
    });
    connect(m_transport, &ClipboardTransport::ownerChanged, this,
            &ClipboardClient::acceptOwner);
    connect(m_transport, &ClipboardTransport::invalidated, this,
            &ClipboardClient::acceptInvalidation);
    connect(m_transport, &ClipboardTransport::snapshotReply, this,
            &ClipboardClient::acceptSnapshot);
    connect(m_transport, &ClipboardTransport::operationReply, this,
            &ClipboardClient::acceptOperation);
}

void ClipboardClient::start()
{
    if (m_state != ClientState::Stopped) {
        return;
    }
    publishState(ClientState::Starting, QStringLiteral("discovering-owner"));
    m_transport->start();
}

void ClipboardClient::stop()
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
    m_transport->stop();
    publishState(ClientState::Stopped, {});
}

void ClipboardClient::setRequestTimeout(int milliseconds)
{
    m_timeoutMs = qBound(10, milliseconds, 60'000);
}

void ClipboardClient::publishState(ClientState state, const QString &reasonCode)
{
    if (m_state == state && m_reasonCode == reasonCode) {
        return;
    }
    m_state = state;
    m_reasonCode = reasonCode;
    Q_EMIT stateChanged(state, reasonCode);
}

void ClipboardClient::acceptOwner(const QString &owner)
{
    if (m_state == ClientState::Stopped || owner == m_owner) {
        return;
    }
    completeUncertain(QStringLiteral("owner-replaced"));
    m_fetchTimer.stop();
    m_fetching = false;
    m_snapshot.reset();
    m_owner = owner;
    if (owner.isEmpty()) {
        publishState(ClientState::Unavailable, QStringLiteral("service-unavailable"));
    } else {
        publishState(ClientState::Starting, QStringLiteral("fetching-snapshot"));
        fetch();
    }
}

void ClipboardClient::fetch()
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

void ClipboardClient::acceptInvalidation(const QString &owner, quint64 epoch,
                                         quint32 generation, quint64 revision)
{
    if (owner != m_owner || owner.isEmpty()) {
        return;
    }
    if (!m_snapshot.has_value() || epoch != m_snapshot->epoch
        || generation != m_snapshot->generation || revision >= m_snapshot->revision) {
        fetch();
    }
}

void ClipboardClient::acceptSnapshot(const QString &owner, quint64 token,
                                     bool transportSuccess, const Snapshot &snapshot,
                                     const QString &reasonCode)
{
    if (!m_fetching || owner != m_owner || token != m_fetchToken) {
        return;
    }
    m_fetchTimer.stop();
    m_fetching = false;
    const ValidationResult validation = validateSnapshot(snapshot);
    bool contradiction = false;
    bool duplicate = false;
    if (m_snapshot.has_value() && snapshot.epoch == m_snapshot->epoch) {
        contradiction = snapshot.generation < m_snapshot->generation
            || (snapshot.generation == m_snapshot->generation
                && snapshot.revision < m_snapshot->revision);
        duplicate = snapshot == *m_snapshot;
        contradiction = contradiction
            || (snapshot.generation == m_snapshot->generation
                && snapshot.revision == m_snapshot->revision && !duplicate);
    }
    if (!transportSuccess || !validation.accepted || contradiction) {
        m_snapshot.reset();
        completeUncertain(QStringLiteral("snapshot-unavailable"));
        publishState(ClientState::Unavailable,
                     transportSuccess ? QStringLiteral("malformed-snapshot") : reasonCode);
        return;
    }
    const bool replaced = m_snapshot.has_value() && snapshot.epoch != m_snapshot->epoch;
    m_snapshot = snapshot;
    publishState(ClientState::Ready, QStringLiteral("ready"));
    if (!duplicate) {
        Q_EMIT snapshotChanged(snapshot);
    }
    if (replaced) {
        completeUncertain(QStringLiteral("authority-replaced"));
    }
    if (m_dirty) {
        fetch();
    }
}

quint64 ClipboardClient::begin(OperationKind kind,
                               const ClipboardModel::EntryId &entry, bool clearAll)
{
    if (m_nextToken == 0 || m_nextToken == std::numeric_limits<quint64>::max()) {
        return 0;
    }
    const quint64 token = m_nextToken++;
    if (m_operation.has_value()) {
        complete(localResult(kind, token, OperationStatus::Busy,
                             QStringLiteral("operation-busy")));
        return token;
    }
    if (m_owner.isEmpty() || !m_snapshot.has_value()) {
        complete(localResult(kind, token, OperationStatus::Rejected,
                             QStringLiteral("unavailable")));
        return token;
    }
    OperationRequest request{.kind = kind,
                             .requestId = token,
                             .expectedEpoch = m_snapshot->epoch,
                             .expectedGeneration = m_snapshot->generation,
                             .expectedRevision = m_snapshot->revision,
                             .entry = entry,
                             .clearAll = clearAll};
    const ValidationResult validation = validateOperationRequest(request);
    if (!validation.accepted) {
        complete(localResult(kind, token, OperationStatus::Rejected,
                             validation.reasonCode));
        return token;
    }
    m_operation = Pending{.token = token, .request = request};
    m_operationTimer.start(m_timeoutMs);
    m_transport->submitOperation(m_owner, token, request);
    return token;
}

quint64 ClipboardClient::select(const ClipboardModel::EntryId &entry)
{
    return begin(OperationKind::Select, entry, false);
}

quint64 ClipboardClient::remove(const ClipboardModel::EntryId &entry)
{
    return begin(OperationKind::Delete, entry, false);
}

quint64 ClipboardClient::clear(bool all)
{
    return begin(OperationKind::Clear, {}, all);
}

quint64 ClipboardClient::copy(const ClipboardModel::EntryId &entry)
{
    return begin(OperationKind::Copy, entry, false);
}

OperationResult ClipboardClient::localResult(OperationKind kind, quint64 requestId,
                                             OperationStatus status,
                                             const QString &reasonCode) const
{
    Snapshot current;
    if (m_snapshot.has_value()) {
        current = *m_snapshot;
    } else {
        current.epoch = 1;
        current.generation = 1;
    }
    return {.kind = kind,
            .status = status,
            .requestId = requestId,
            .initiatingEpoch = current.epoch,
            .initiatingGeneration = current.generation,
            .initiatingRevision = current.revision,
            .observedEpoch = current.epoch,
            .observedGeneration = current.generation,
            .observedRevision = current.revision,
            .reasonCode = reasonCode};
}

void ClipboardClient::complete(OperationResult result)
{
    QMetaObject::invokeMethod(this, [this, result] {
        Q_EMIT operationCompleted(result.requestId, result);
    }, Qt::QueuedConnection);
}

void ClipboardClient::completeUncertain(const QString &reasonCode)
{
    if (!m_operation.has_value()) {
        return;
    }
    const Pending pending = *m_operation;
    m_operation.reset();
    m_operationTimer.stop();
    complete(localResult(pending.request.kind, pending.request.requestId,
                         OperationStatus::Uncertain, reasonCode));
}

void ClipboardClient::acceptOperation(const QString &owner, quint64 token,
                                      bool transportSuccess, const OperationResult &result,
                                      const QString &reasonCode)
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
        && result.initiatingEpoch == pending.request.expectedEpoch
        && result.initiatingGeneration == pending.request.expectedGeneration
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

} // namespace QindaQt::Services::Clipboard

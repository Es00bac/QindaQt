// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_client/power_client.h>

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <limits>

namespace QindaQt::Power {

PowerClient::PowerClient(PowerTransport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
    Q_ASSERT(m_transport != nullptr);
    m_fetchTimer.setSingleShot(true);
    m_operationTimer.setSingleShot(true);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(200);
    connect(&m_fetchTimer, &QTimer::timeout, this, &PowerClient::onFetchTimeout);
    connect(&m_operationTimer, &QTimer::timeout, this,
            &PowerClient::onOperationTimeout);
    connect(&m_retryTimer, &QTimer::timeout, this, &PowerClient::requestSnapshot);
    connect(m_transport, &PowerTransport::ownerChanged, this,
            &PowerClient::acceptOwner);
    connect(m_transport, &PowerTransport::invalidated, this,
            &PowerClient::acceptInvalidation);
    connect(m_transport, &PowerTransport::snapshotReply, this,
            &PowerClient::acceptSnapshotReply);
    connect(m_transport, &PowerTransport::operationReply, this,
            &PowerClient::acceptOperationReply);
}

void PowerClient::start()
{
    if (m_state != PowerClientState::Stopped) {
        return;
    }
    publishState(PowerClientState::Starting, QStringLiteral("discovering-owner"));
    m_transport->start();
}

void PowerClient::stop()
{
    if (m_state == PowerClientState::Stopped) {
        return;
    }
    // AGENT-GUARD: Results accepted before stop but not yet published belong to
    // the cancelled client lifetime. Drop those first, but retain the distinct
    // asynchronous Uncertain result created here for a mutation that is still
    // transport-backed. The receiver-context queue drops it on destruction.
    cancelQueuedOperationCompletions();
    completeUncertain(QStringLiteral("client-stopped"));
    m_fetchTimer.stop();
    m_operationTimer.stop();
    m_retryTimer.stop();
    m_fetchInFlight = false;
    m_refetchNeeded = false;
    m_fetchRequestId = 0;
    m_snapshot.reset();
    m_owner.clear();
    m_transport->stop();
    publishState(PowerClientState::Stopped, {});
}

PowerClientState PowerClient::state() const noexcept
{
    return m_state;
}

QString PowerClient::reasonCode() const
{
    return m_reasonCode;
}

QString PowerClient::owner() const
{
    return m_owner;
}

bool PowerClient::hasSnapshot() const noexcept
{
    return m_snapshot.has_value();
}

Snapshot PowerClient::snapshot() const
{
    return m_snapshot.value_or(Snapshot{});
}

bool PowerClient::operationPending() const noexcept
{
    return m_operation.has_value();
}

void PowerClient::setRequestTimeout(const int milliseconds)
{
    m_requestTimeoutMs = qBound(10, milliseconds, 60'000);
}

void PowerClient::publishState(const PowerClientState state,
                               const QString &reasonCode)
{
    if (m_state == state && m_reasonCode == reasonCode) {
        return;
    }
    m_state = state;
    m_reasonCode = reasonCode;
    Q_EMIT stateChanged(m_state, m_reasonCode);
}

void PowerClient::publishSnapshotState(const Snapshot &snapshot)
{
    switch (snapshot.availability) {
    case Availability::Starting:
        publishState(PowerClientState::Starting, snapshot.reasonCode);
        break;
    case Availability::Ready:
        publishState(PowerClientState::Ready, snapshot.reasonCode);
        break;
    case Availability::Unavailable:
        publishState(PowerClientState::Unavailable, snapshot.reasonCode);
        break;
    case Availability::Degraded:
        publishState(PowerClientState::Degraded, snapshot.reasonCode);
        break;
    }
}

void PowerClient::acceptOwner(const QString &owner)
{
    if (m_state == PowerClientState::Stopped || owner == m_owner) {
        return;
    }
    // AGENT-CONTRACT: A D-Bus unique-owner change is an independent client
    // authority change. The snapshot is discarded, an in-flight mutation is
    // completed Uncertain, and truth is refetched from the new exact owner
    // rather than replayed.
    completeUncertain(QStringLiteral("owner-replaced"));
    m_fetchTimer.stop();
    m_retryTimer.stop();
    m_fetchInFlight = false;
    m_refetchNeeded = false;
    m_fetchRequestId = 0;
    m_snapshot.reset();
    m_owner = owner;
    if (m_owner.isEmpty()) {
        publishState(PowerClientState::Unavailable,
                     QStringLiteral("service-unavailable"));
        return;
    }
    publishState(PowerClientState::Starting, QStringLiteral("fetching-snapshot"));
    requestSnapshot();
}

void PowerClient::requestSnapshot()
{
    if (m_owner.isEmpty() || m_state == PowerClientState::Stopped) {
        return;
    }
    if (m_fetchInFlight) {
        m_refetchNeeded = true;
        return;
    }
    if (m_nextRequestId == 0
        || m_nextRequestId == std::numeric_limits<quint64>::max()) {
        publishState(PowerClientState::Unavailable,
                     QStringLiteral("request-id-exhausted"));
        return;
    }
    m_fetchInFlight = true;
    m_refetchNeeded = false;
    m_fetchRequestId = m_nextRequestId++;
    m_fetchTimer.start(m_requestTimeoutMs);
    m_transport->fetchSnapshot(m_owner, m_fetchRequestId);
}

void PowerClient::scheduleRefetch()
{
    if (!m_retryTimer.isActive() && !m_owner.isEmpty()) {
        m_retryTimer.start();
    }
}

void PowerClient::acceptInvalidation(const QString &owner, const quint64 epoch,
                                     const quint64 revision)
{
    if (owner != m_owner || owner.isEmpty()) {
        return;
    }
    if (!m_snapshot.has_value() || epoch != m_snapshot->epoch
        || revision > m_snapshot->revision) {
        requestSnapshot();
    }
}

void PowerClient::acceptSnapshotReply(const QString &owner, const quint64 requestId,
                                      const bool transportSuccess,
                                      const Snapshot &snapshot,
                                      const QString &reasonCode)
{
    if (!m_fetchInFlight || owner != m_owner || requestId != m_fetchRequestId) {
        return;
    }
    m_fetchTimer.stop();
    m_fetchInFlight = false;
    m_fetchRequestId = 0;

    const ValidationResult validation = validateSnapshot(snapshot);
    // AGENT-GUARD: Epochs only increase within one exact owner. An equal-owner
    // regressed epoch, a regressed revision, or an equal-lineage content
    // contradiction is hostile or stale input and is never published.
    bool lineageContradiction = false;
    bool exactDuplicate = false;
    if (m_snapshot.has_value()) {
        if (snapshot.epoch < m_snapshot->epoch) {
            lineageContradiction = true;
        } else if (snapshot.epoch == m_snapshot->epoch) {
            if (snapshot.revision < m_snapshot->revision) {
                lineageContradiction = true;
            } else if (snapshot.revision == m_snapshot->revision) {
                exactDuplicate = snapshot == *m_snapshot;
                lineageContradiction = !exactDuplicate;
            }
        }
    }
    if (!transportSuccess || !validation.accepted || lineageContradiction) {
        publishState(PowerClientState::Unavailable,
                     !transportSuccess ? reasonCode
                                       : QStringLiteral("malformed-snapshot"));
        scheduleRefetch();
        return;
    }

    if (exactDuplicate) {
        publishSnapshotState(snapshot);
        if (m_refetchNeeded) {
            requestSnapshot();
        }
        return;
    }
    const bool authorityReplaced =
        m_snapshot.has_value() && snapshot.epoch != m_snapshot->epoch;
    m_snapshot = snapshot;
    publishSnapshotState(snapshot);
    Q_EMIT snapshotChanged(snapshot);
    if (authorityReplaced) {
        // AGENT-CONTRACT: A new accepted epoch retires the dispatched mutation
        // immediately; a delayed old-epoch reply can never restore success or
        // trigger a replay. The caller resnapshots instead.
        completeUncertain(QStringLiteral("authority-replaced"));
    }
    if (m_refetchNeeded) {
        requestSnapshot();
    }
}

void PowerClient::onFetchTimeout()
{
    if (!m_fetchInFlight) {
        return;
    }
    m_fetchInFlight = false;
    m_fetchRequestId = 0;
    publishState(PowerClientState::Unavailable, QStringLiteral("snapshot-timeout"));
    scheduleRefetch();
}

void PowerClient::onOperationTimeout()
{
    completeUncertain(QStringLiteral("operation-timeout"));
    requestSnapshot();
}

} // namespace QindaQt::Power

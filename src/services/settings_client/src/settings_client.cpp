// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_client/settings_client.h"

#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include "settings_reply_validation_p.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace QindaQt::Services::SettingsClient {
namespace {

using namespace SettingsProtocol;
constexpr int MaximumDelay = 60'000;

void setError(QString *output, QString message)
{
    if (output != nullptr) {
        *output = std::move(message);
    }
}

} // namespace

using Private::boundedSourceMap;
using Private::boundedValueMap;
using Private::exactUnsigned64;
using Private::validEpoch;
using Private::validVersions;
using Private::wireStatus;

bool ClientTiming::isValid() const noexcept
{
    if (requestTimeoutMilliseconds <= 0 || requestTimeoutMilliseconds > MaximumDelay
        || debounceMilliseconds < 0 || debounceMilliseconds > MaximumDelay
        || retryMilliseconds.isEmpty()) {
        return false;
    }
    int previous = 0;
    for (const int delay : retryMilliseconds) {
        if (delay <= 0 || delay > MaximumDelay || delay < previous) {
            return false;
        }
        previous = delay;
    }
    return true;
}

SettingsClient::SettingsClient(SettingsTransport &transport, QStringList scopedKeys,
                               ClientTiming timing, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_keys(std::move(scopedKeys))
    , m_timing(std::move(timing))
{
    m_refreshTimer.setSingleShot(true);
    m_timeout.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &SettingsClient::requestSnapshotNow);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (!m_request) {
            return;
        }
        if (m_request->kind == RequestKind::Commit) {
            makeWriteUncertain(QStringLiteral("settings commit timed out"));
            return;
        }
        m_request.reset();
        publish(ClientState::Degraded, QStringLiteral("settings snapshot timed out"));
        scheduleRetry();
    });
    connect(&m_transport, &SettingsTransport::ownerChanged,
            this, &SettingsClient::handleOwnerChanged);
    connect(&m_transport, &SettingsTransport::settingsChanged,
            this, &SettingsClient::handleInvalidation);
    connect(&m_transport, &SettingsTransport::snapshotReceived,
            this, &SettingsClient::handleSnapshot);
    connect(&m_transport, &SettingsTransport::commitReceived,
            this, &SettingsClient::handleCommit);
    connect(&m_transport, &SettingsTransport::requestFailed,
            this, &SettingsClient::handleFailure);
    connect(&m_transport, &SettingsTransport::activationFailed, this, [this](const QString &message) {
        if (m_owner.isEmpty()) {
            publish(ClientState::Unavailable, message.left(512));
            scheduleRetry();
        }
    });
    connect(&m_transport, &SettingsTransport::busDisconnected,
            this, &SettingsClient::handleBusDisconnected);
}

SettingsClient::~SettingsClient()
{
    stop();
}

bool SettingsClient::start(QString *error)
{
    if (m_started) {
        setError(error, {});
        return true;
    }
    QSet<QString> unique;
    if (!m_timing.isValid() || m_keys.isEmpty()
        || m_keys.size() > WireContract::MaximumRequestedKeys) {
        setError(error, QStringLiteral("settings client scope or timing is invalid"));
        return false;
    }
    for (const auto &key : m_keys) {
        if (!BoundedSettingsValueCodec::validateKey(key, error) || unique.contains(key)) {
            setError(error, QStringLiteral("settings client scope has an invalid or duplicate key"));
            return false;
        }
        unique.insert(key);
    }
    m_started = true;
    if (!m_transport.start(error)) {
        m_started = false;
        return false;
    }
    m_transport.requestActivation();
    return true;
}

void SettingsClient::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    m_refreshTimer.stop();
    m_timeout.stop();
    m_request.reset();
    const bool wasWriting = m_write.has_value();
    m_write.reset();
    m_snapshot.reset();
    m_owner.clear();
    m_targetRevision = 0;
    m_dirty = false;
    m_retryIndex = 0;
    m_transport.stop();
    publish(ClientState::Unavailable);
    if (wasWriting) {
        Q_EMIT writeInFlightChanged();
    }
}

void SettingsClient::refresh()
{
    if (!m_started) {
        return;
    }
    if (m_owner.isEmpty()) {
        m_transport.requestActivation();
        return;
    }
    if (m_request) {
        m_dirty = true;
        return;
    }
    m_refreshTimer.start(0);
}

bool SettingsClient::setUserValue(const QString &key, const QVariant &value, QString *error)
{
    if (m_state != ClientState::Ready || !m_snapshot || m_request || m_write
        || !m_keys.contains(key)) {
        setError(error, QStringLiteral("settings client is not ready for this key"));
        return false;
    }
    if (!BoundedSettingsValueCodec::validateValue(value, error)) {
        return false;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        setError(error, QStringLiteral("settings request token is exhausted"));
        return false;
    }
    const QVariantMap operation{{QLatin1StringView(WireContract::FieldKey), key},
                                {QLatin1StringView(WireContract::FieldKind),
                                 QLatin1StringView(WireContract::OperationKindSet)},
                                {QLatin1StringView(WireContract::FieldValue), value}};
    m_write = Write{key, value, false};
    m_request = Request{token, m_owner, RequestKind::Commit};
    m_timeout.start(m_timing.requestTimeoutMilliseconds);
    Q_EMIT writeInFlightChanged();
    m_transport.commit(token, m_owner, m_snapshot->epoch, m_snapshot->revision,
                       QVariantList{operation});
    return true;
}

bool SettingsClient::removeUserValue(const QString &key, QString *error)
{
    if (m_state != ClientState::Ready || !m_snapshot || m_request || m_write
        || !m_keys.contains(key)) {
        setError(error, QStringLiteral("settings client is not ready for this key"));
        return false;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        setError(error, QStringLiteral("settings request token is exhausted"));
        return false;
    }
    const QVariantMap operation{{QLatin1StringView(WireContract::FieldKey), key},
                                {QLatin1StringView(WireContract::FieldKind),
                                 QLatin1StringView(WireContract::OperationKindRemove)}};
    m_write = Write{key, {}, true};
    m_request = Request{token, m_owner, RequestKind::Commit};
    m_timeout.start(m_timing.requestTimeoutMilliseconds);
    Q_EMIT writeInFlightChanged();
    m_transport.commit(token, m_owner, m_snapshot->epoch, m_snapshot->revision,
                       QVariantList{operation});
    return true;
}

void SettingsClient::handleOwnerChanged(const QString &owner)
{
    if (!m_started || owner == m_owner) {
        return;
    }
    m_refreshTimer.stop();
    m_timeout.stop();
    const bool interruptedWrite = m_write.has_value();
    m_request.reset();
    m_write.reset();
    m_owner = owner;
    m_targetRevision = 0;
    m_dirty = false;
    m_retryIndex = 0;
    if (owner.isEmpty()) {
        publish(ClientState::Unavailable, QStringLiteral("settings service is unavailable"));
        m_transport.requestActivation();
    } else {
        publish(ClientState::Authenticating);
        m_refreshTimer.start(0);
    }
    if (interruptedWrite) {
        Q_EMIT writeInFlightChanged();
        Q_EMIT commitUncertain(QStringLiteral("settings service changed during commit"));
    }
}

void SettingsClient::handleInvalidation(const QString &owner, const QString &epoch,
                                        quint64 revision, const QStringList &keys)
{
    if (!m_started || owner != m_owner || !m_snapshot || epoch != m_snapshot->epoch
        || revision <= m_snapshot->revision) {
        return;
    }
    bool relevant = false;
    for (const auto &key : keys) {
        relevant = relevant || m_keys.contains(key);
    }
    if (!relevant) {
        return;
    }
    m_targetRevision = std::max(m_targetRevision, revision);
    if (m_request) {
        m_dirty = true;
    } else {
        m_refreshTimer.start(m_timing.debounceMilliseconds);
    }
}

void SettingsClient::handleSnapshot(quint64 token, const QString &owner,
                                    const QVariantMap &wire)
{
    if (!m_request || m_request->kind != RequestKind::Snapshot
        || m_request->token != token || m_request->owner != owner || owner != m_owner) {
        return;
    }
    m_request.reset();
    m_timeout.stop();
    const auto replyStatus = wireStatus(wire);
    const auto revision = exactUnsigned64(wire.value(QLatin1StringView(WireContract::FieldRevision)));
    const QString epoch = wire.value(QLatin1StringView(WireContract::FieldEpoch)).toString();
    const auto values = boundedValueMap(wire.value(QLatin1StringView(WireContract::FieldValues)));
    const auto sources = boundedSourceMap(wire.value(QLatin1StringView(WireContract::FieldSourceLayers)));
    quint32 settingsSchemaVersion = 0;
    bool exactScope = values && sources && values->size() == m_keys.size()
                      && sources->size() == m_keys.size();
    for (const auto &key : m_keys) {
        exactScope = exactScope && values && sources && values->contains(key) && sources->contains(key);
    }
    if (!replyStatus || *replyStatus != SettingsWireStatus::Applied || !revision
        || !validVersions(wire, &settingsSchemaVersion) || !validEpoch(epoch) || !exactScope
        || (m_snapshot && m_snapshot->owner == owner && m_snapshot->epoch == epoch
            && *revision < m_snapshot->revision)) {
        publish(ClientState::Degraded, QStringLiteral("settings snapshot is malformed or regressed"));
        scheduleRetry();
        return;
    }
    m_snapshot = SettingsSnapshot{owner, epoch, settingsSchemaVersion, *revision, *values, *sources};
    m_retryIndex = 0;
    publish(ClientState::Ready);
    Q_EMIT snapshotChanged();
    const bool followUp = m_dirty || m_targetRevision > *revision;
    m_dirty = false;
    if (m_targetRevision <= *revision) {
        m_targetRevision = 0;
    }
    if (followUp) {
        m_refreshTimer.start(0);
    }
}

void SettingsClient::handleCommit(quint64 token, const QString &owner,
                                  const QVariantMap &wire)
{
    if (!m_request || m_request->kind != RequestKind::Commit || !m_write
        || m_request->token != token || m_request->owner != owner || owner != m_owner) {
        return;
    }
    m_request.reset();
    m_timeout.stop();
    const auto replyStatus = wireStatus(wire);
    const auto before = exactUnsigned64(wire.value(QLatin1StringView(WireContract::FieldRevisionBefore)));
    const auto after = exactUnsigned64(wire.value(QLatin1StringView(WireContract::FieldRevisionAfter)));
    const auto values = boundedValueMap(wire.value(QLatin1StringView(WireContract::FieldValues)));
    const auto sources = boundedSourceMap(wire.value(QLatin1StringView(WireContract::FieldSourceLayers)));
    const QString message = wire.value(QLatin1StringView(WireContract::FieldMessage)).toString().left(512);
    const QStringList changed = wire.value(QLatin1StringView(WireContract::FieldChangedKeys)).toStringList();
    quint32 settingsSchemaVersion = 0;
    const QString writeKey = m_write->key;
    m_write.reset();
    Q_EMIT writeInFlightChanged();
    if (!replyStatus || !before || !after || !values || !sources
        || !validVersions(wire, &settingsSchemaVersion)
        || !values->contains(writeKey) || !sources->contains(writeKey)) {
        publish(ClientState::Degraded, QStringLiteral("settings commit reply is malformed"));
        Q_EMIT commitUncertain(m_lastError);
        refresh();
        return;
    }
    CommitOutcome outcome{*replyStatus, *before, *after, *values, *sources, changed, message};
    Q_EMIT commitFinished(outcome);
    // Even a confirmed rejection can race an invalidation. Re-read authority;
    // clients never manufacture a partial snapshot from a commit reply.
    publish(ClientState::Authenticating,
            *replyStatus == SettingsWireStatus::Applied ? QString{} : message);
    refresh();
}

void SettingsClient::handleFailure(quint64 token, const QString &owner,
                                   const QString &, const QString &message)
{
    if (!m_request || m_request->token != token || m_request->owner != owner || owner != m_owner) {
        return;
    }
    if (m_request->kind == RequestKind::Commit) {
        makeWriteUncertain(message.isEmpty() ? QStringLiteral("settings commit transport failed")
                                             : message.left(512));
        return;
    }
    m_request.reset();
    m_timeout.stop();
    publish(ClientState::Degraded, message.left(512));
    scheduleRetry();
}

void SettingsClient::handleBusDisconnected()
{
    if (!m_started) {
        return;
    }
    const bool uncertain = m_write.has_value();
    m_refreshTimer.stop();
    m_timeout.stop();
    m_request.reset();
    m_write.reset();
    m_owner.clear();
    publish(ClientState::Unavailable, QStringLiteral("session bus disconnected"));
    if (uncertain) {
        Q_EMIT writeInFlightChanged();
        Q_EMIT commitUncertain(m_lastError);
    }
}

void SettingsClient::requestSnapshotNow()
{
    if (!m_started || m_owner.isEmpty() || m_request) {
        return;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        publish(ClientState::Degraded, QStringLiteral("settings request token is exhausted"));
        return;
    }
    m_request = Request{token, m_owner, RequestKind::Snapshot};
    m_timeout.start(m_timing.requestTimeoutMilliseconds);
    m_transport.requestSnapshot(token, m_owner, m_keys);
}

void SettingsClient::scheduleRetry()
{
    if (!m_started) {
        return;
    }
    if (m_owner.isEmpty()) {
        m_transport.requestActivation();
        return;
    }
    const qsizetype last = m_timing.retryMilliseconds.size() - 1;
    const qsizetype index = std::min(m_retryIndex, last);
    if (m_retryIndex < last) {
        ++m_retryIndex;
    }
    m_refreshTimer.start(m_timing.retryMilliseconds.at(index));
}

void SettingsClient::makeWriteUncertain(QString message)
{
    m_timeout.stop();
    m_request.reset();
    m_write.reset();
    Q_EMIT writeInFlightChanged();
    publish(ClientState::Degraded, std::move(message));
    Q_EMIT commitUncertain(m_lastError);
    refresh();
}

void SettingsClient::publish(ClientState state, QString error)
{
    if (m_state == state && m_lastError == error) {
        return;
    }
    m_state = state;
    m_lastError = std::move(error);
    Q_EMIT stateChanged();
}

quint64 SettingsClient::nextToken()
{
    if (m_nextToken == 0) {
        return 0;
    }
    const quint64 result = m_nextToken++;
    if (m_nextToken == 0) {
        m_nextToken = 0;
    }
    return result;
}

} // namespace QindaQt::Services::SettingsClient

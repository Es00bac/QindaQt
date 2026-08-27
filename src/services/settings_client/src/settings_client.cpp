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
using Private::boundedChangedKeys;
using Private::boundedValueMap;
using Private::boundedWireMessage;
using Private::exactUnsigned64;
using Private::hasExactSnapshotFields;
using Private::validEpoch;
using Private::validVersions;
using Private::validatedCommitReply;
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
    connect(&m_refreshTimer, &QTimer::timeout, this, &SettingsClient::handleRefreshTimer);
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
    connect(&m_transport, &SettingsTransport::activationCompleted,
            this, &SettingsClient::handleActivationCompleted);
    connect(&m_transport, &SettingsTransport::activationFailed,
            this, &SettingsClient::handleActivationFailure);
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
        if (!startTransport(error)) {
            return false;
        }
        requestActivationIfReady();
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
    if (!startTransport(error)) {
        return false;
    }
    requestActivationIfReady();
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
    m_dirty = false;
    m_retryIndex = 0;
    m_activationInFlight = false;
    if (m_transportStarted) {
        m_transport.stop();
    }
    m_transportStarted = false;
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
    if (!m_transportStarted) {
        QString error;
        if (!startTransport(&error)) {
            return;
        }
    }
    if (m_owner.isEmpty()) {
        requestActivationIfReady();
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
    m_request = Request{token, m_owner, RequestKind::Commit, m_snapshot->epoch,
                        m_snapshot->settingsSchemaVersion, m_snapshot->revision};
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
    m_request = Request{token, m_owner, RequestKind::Commit, m_snapshot->epoch,
                        m_snapshot->settingsSchemaVersion, m_snapshot->revision};
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
    m_dirty = false;
    m_activationInFlight = false;
    if (owner.isEmpty()) {
        publish(ClientState::Unavailable, QStringLiteral("settings service is unavailable"));
        scheduleRetry();
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
    const auto boundedKeys = boundedChangedKeys(QVariant::fromValue(keys));
    if (!m_started || owner != m_owner || !m_snapshot || !validEpoch(epoch)
        || epoch != m_snapshot->epoch || !boundedKeys || boundedKeys->isEmpty()
        || revision <= m_snapshot->revision) {
        return;
    }
    // Settings1 revisions are repository-global. Even a transaction touching
    // only another scope invalidates this client's commit base; skipping that
    // refresh would make its next legitimate write conflict spuriously.
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
    const auto message = boundedWireMessage(
        wire.value(QLatin1StringView(WireContract::FieldMessage)));
    const QVariant epochField = wire.value(QLatin1StringView(WireContract::FieldEpoch));
    quint32 settingsSchemaVersion = 0;
    bool exactScope = values && sources && values->size() == m_keys.size()
                      && sources->size() == m_keys.size();
    for (const auto &key : m_keys) {
        exactScope = exactScope && values && sources && values->contains(key) && sources->contains(key);
    }
    if (!hasExactSnapshotFields(wire)
        || !replyStatus || *replyStatus != SettingsWireStatus::Applied || !revision || !message
        || epochField.metaType().id() != QMetaType::QString
        || !validVersions(wire, &settingsSchemaVersion) || !validEpoch(epoch) || !exactScope
        || (m_snapshot && m_snapshot->owner == owner
            && (epoch != m_snapshot->epoch
                || settingsSchemaVersion != m_snapshot->settingsSchemaVersion
                || *revision < m_snapshot->revision
                || (*revision == m_snapshot->revision
                    && (*values != m_snapshot->values
                        || *sources != m_snapshot->sourceLayers))))) {
        publish(ClientState::Degraded, QStringLiteral("settings snapshot is malformed or regressed"));
        scheduleRetry();
        return;
    }
    m_snapshot = SettingsSnapshot{owner, epoch, settingsSchemaVersion, *revision, *values, *sources};
    m_retryIndex = 0;
    publish(ClientState::Ready);
    Q_EMIT snapshotChanged();
    const bool followUp = m_dirty;
    m_dirty = false;
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
    const Request request = *m_request;
    const Write write = *m_write;
    m_request.reset();
    m_timeout.stop();
    const auto outcome = validatedCommitReply(
        wire, Private::CommitReplyContext{request.epoch, request.settingsSchemaVersion,
                                         request.baseRevision, write.key});
    m_write.reset();
    Q_EMIT writeInFlightChanged();
    if (!outcome) {
        publish(ClientState::Degraded, QStringLiteral("settings commit reply is malformed"));
        Q_EMIT commitUncertain(m_lastError);
        refresh();
        return;
    }
    Q_EMIT commitFinished(*outcome);
    // Even a confirmed rejection can race an invalidation. Re-read authority;
    // clients never manufacture a partial snapshot from a commit reply.
    publish(ClientState::Authenticating,
            outcome->status == SettingsWireStatus::Applied ? QString{} : outcome->message);
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
    m_transportStarted = false;
    m_activationInFlight = false;
    publish(ClientState::Unavailable, QStringLiteral("session bus disconnected"));
    if (uncertain) {
        Q_EMIT writeInFlightChanged();
        Q_EMIT commitUncertain(m_lastError);
    }
}

void SettingsClient::handleActivationFailure(const QString &message)
{
    if (!m_started || !m_activationInFlight || !m_owner.isEmpty()) {
        return;
    }
    m_activationInFlight = false;
    publish(ClientState::Unavailable,
            message.isEmpty() ? QStringLiteral("settings activation failed")
                              : message.left(512));
    scheduleRetry();
}

void SettingsClient::handleActivationCompleted()
{
    if (!m_started || !m_activationInFlight || !m_owner.isEmpty()) {
        return;
    }
    // A successful StartServiceByName reply is not an owner baseline. The
    // process may have exited already; release the serialized attempt and let
    // owner resolution race a bounded retry instead of stranding activation.
    m_activationInFlight = false;
    publish(ClientState::Unavailable,
            QStringLiteral("settings activation completed without a stable owner"));
    scheduleRetry();
}

void SettingsClient::handleRefreshTimer()
{
    if (m_owner.isEmpty()) {
        requestActivationIfReady();
    } else {
        requestSnapshotNow();
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
    // Only invalidations that arrive after this call starts require a second
    // baseline. A prior signal is an ordering hint, not an attacker-controlled
    // target revision that can force an endless catch-up loop.
    m_dirty = false;
    m_request = Request{token, m_owner, RequestKind::Snapshot, {}, 0, 0};
    m_timeout.start(m_timing.requestTimeoutMilliseconds);
    m_transport.requestSnapshot(token, m_owner, m_keys);
}

void SettingsClient::requestActivationIfReady()
{
    if (!m_started || !m_transportStarted || !m_owner.isEmpty()
        || m_activationInFlight || m_refreshTimer.isActive()) {
        return;
    }
    m_activationInFlight = true;
    m_transport.requestActivation();
}

bool SettingsClient::startTransport(QString *error)
{
    if (m_transportStarted) {
        setError(error, {});
        return true;
    }
    if (!m_transport.start(error)) {
        const QString message = error != nullptr && !error->isEmpty()
                                    ? error->left(512)
                                    : QStringLiteral("settings transport could not start");
        publish(ClientState::Unavailable, message);
        return false;
    }
    m_transportStarted = true;
    publish(ClientState::Authenticating);
    return true;
}

void SettingsClient::scheduleRetry()
{
    if (!m_started) {
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

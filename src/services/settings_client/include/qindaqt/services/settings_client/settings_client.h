// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Services::SettingsClient {

class SettingsTransport;

enum class ClientState { Unavailable, Authenticating, Ready, Degraded };

struct ClientTiming final {
    int requestTimeoutMilliseconds = 2'000;
    int debounceMilliseconds = 16;
    QVector<int> retryMilliseconds{100, 250, 500, 1'000, 2'000, 5'000};
    [[nodiscard]] bool isValid() const noexcept;
};

struct SettingsSnapshot final {
    QString owner;
    QString epoch;
    quint32 settingsSchemaVersion = 0;
    quint64 revision = 0;
    QVariantMap values;
    QVariantMap sourceLayers;
};

struct CommitOutcome final {
    SettingsProtocol::SettingsWireStatus status =
        SettingsProtocol::SettingsWireStatus::MalformedRequest;
    quint64 revisionBefore = 0;
    quint64 revisionAfter = 0;
    QVariantMap currentValues;
    QVariantMap currentSourceLayers;
    QStringList changedKeys;
    QString message;
};

// Owns one exact Settings1 owner/epoch lineage. All calls are asynchronous;
// timeout, owner replacement, and local bus loss fence late replies. An
// uncertain write is never automatically replayed.
class SettingsClient final : public QObject {
    Q_OBJECT
public:
    SettingsClient(SettingsTransport &transport, QStringList scopedKeys,
                   ClientTiming timing = {}, QObject *parent = nullptr);
    ~SettingsClient() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    void refresh();
    [[nodiscard]] bool setUserValue(const QString &key, const QVariant &value,
                                    QString *error = nullptr);
    [[nodiscard]] bool removeUserValue(const QString &key, QString *error = nullptr);

    [[nodiscard]] ClientState state() const noexcept { return m_state; }
    [[nodiscard]] const QString &lastError() const noexcept { return m_lastError; }
    // Currently observed exact transport owner, independent of the last confirmed
    // snapshot; this owner may not yet have a confirmed baseline. Empty after
    // owner loss. Same-thread borrowed reference valid
    // only while this client lives; compare it before trusting a retained
    // snapshot during an asynchronous refresh.
    [[nodiscard]] const QString &currentOwner() const noexcept { return m_owner; }
    [[nodiscard]] const std::optional<SettingsSnapshot> &snapshot() const noexcept { return m_snapshot; }
    [[nodiscard]] bool writeInFlight() const noexcept { return m_write.has_value(); }
    // Same-thread admission preview for a known-valid value. It mirrors the
    // state, scope, request, and token guard in setUserValue(); callers must
    // still check that method's return because validation and races can fail.
    // No ownership is transferred and the answer is valid only until the next
    // client/transport event. See ADR-0249.
    [[nodiscard]] bool canSetUserValue(const QString &key) const noexcept {
        return m_state == ClientState::Ready && m_snapshot && !m_request && !m_write
               && m_nextToken != 0 && m_keys.contains(key);
    }

Q_SIGNALS:
    void stateChanged();
    // Emitted synchronously after the exact owner changes, even if state and
    // error stay unchanged. currentOwner() is new; snapshot() may still be
    // last-known authority from the old owner.
    void ownerChanged();
    void snapshotChanged();
    void writeInFlightChanged();
    // Emitted when a readiness, request, or token transition may change
    // canSetUserValue() for any scoped key.
    void writeAdmissionChanged();
    void commitFinished(const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
    void commitUncertain(const QString &message);

private:
    enum class RequestKind { Snapshot, Commit };
    struct Request final {
        quint64 token = 0;
        QString owner;
        RequestKind kind = RequestKind::Snapshot;
        QString epoch;
        quint32 settingsSchemaVersion = 0;
        quint64 baseRevision = 0;
    };
    struct Write final { QString key; QVariant value; bool remove = false; };

    void setOwner(QString owner);
    void handleOwnerChanged(const QString &owner);
    void handleInvalidation(const QString &owner, const QString &epoch,
                            quint64 revision, const QStringList &keys);
    void handleSnapshot(quint64 token, const QString &owner, const QVariantMap &wire);
    void handleCommit(quint64 token, const QString &owner, const QVariantMap &wire);
    void handleFailure(quint64 token, const QString &owner,
                       const QString &errorName, const QString &message);
    void handleActivationCompleted();
    void handleActivationFailure(const QString &message);
    void handleBusDisconnected();
    void handleRefreshTimer();
    void requestSnapshotNow();
    void requestActivationIfReady();
    [[nodiscard]] bool startTransport(QString *error = nullptr);
    void scheduleRetry();
    void makeWriteUncertain(QString message);
    void publish(ClientState state, QString error = {});
    [[nodiscard]] quint64 nextToken() noexcept {
        if (m_nextToken == 0) return 0;
        return m_nextToken++;
    }

    SettingsTransport &m_transport;
    QStringList m_keys;
    ClientTiming m_timing;
    QTimer m_refreshTimer;
    QTimer m_timeout;
    std::optional<Request> m_request;
    std::optional<Write> m_write;
    std::optional<SettingsSnapshot> m_snapshot;
    QString m_owner;
    QString m_lastError;
    ClientState m_state = ClientState::Unavailable;
    qsizetype m_retryIndex = 0;
    quint64 m_nextToken = 1;
    bool m_started = false;
    bool m_transportStarted = false;
    bool m_activationInFlight = false;
    bool m_dirty = false;
};

} // namespace QindaQt::Services::SettingsClient

Q_DECLARE_METATYPE(QindaQt::Services::SettingsClient::CommitOutcome)

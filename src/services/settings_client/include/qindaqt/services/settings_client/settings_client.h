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
    [[nodiscard]] const std::optional<SettingsSnapshot> &snapshot() const noexcept { return m_snapshot; }
    [[nodiscard]] bool writeInFlight() const noexcept { return m_write.has_value(); }

Q_SIGNALS:
    void stateChanged();
    void snapshotChanged();
    void writeInFlightChanged();
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
    [[nodiscard]] quint64 nextToken();

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

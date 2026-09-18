// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/obs_client/obs_protocol.h>
#include <qindaqt/services/obs_client/obs_transport.h>
#include <qindaqt/services/obs_client/obs_types.h>

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

namespace QindaQt::Obs {

// Backoff and timeout policy, injectable so a row does not wait real seconds.
struct ClientTiming {
    // Reconnect delays, in order; the last one repeats. OBS is a desktop
    // application the user starts and stops, so the client keeps trying at a
    // slow cadence rather than giving up.
    QList<int> reconnectMilliseconds{500, 1000, 2000, 5000, 10000};
    int requestTimeoutMilliseconds = 5000;
    // How often the client re-reads an ACTIVE output's statistics. OBS
    // publishes events for start and stop but none for elapsed time or
    // dropped frames, so those numbers exist only in GetRecordStatus and
    // GetStreamStatus. The timer runs only while an output is active.
    int statisticsIntervalMilliseconds = 2000;
    [[nodiscard]] bool isValid() const;
};

// The desktop's obs-websocket v5 client.
//
// AGENT-CONTRACT: One connection, one pending-request table keyed by an id
// this client generates, and one published snapshot. Every operation is
// asynchronous and reports through `operationFinished`. State comes from
// OBS's events after the first full read, so an OBS the user drives directly
// stays reflected in the top bar.
//
// The one exception to "no polling": elapsed time and dropped frames are not
// published by any obs-websocket event, so while an output is ACTIVE the
// client re-reads that output's status on `statisticsIntervalMilliseconds`.
// Nothing polls while OBS is idle.
//
// AGENT-GUARD: The password is held only to answer a Hello challenge. It is
// never published in the snapshot, never logged, and never sent anywhere but
// the authentication digest.
class ObsClient final : public QObject {
    Q_OBJECT

public:
    // One completed operation. `requestType` is the OBS request name so a
    // surface can tell which toggle answered.
    struct OperationResult {
        quint64 requestId = 0;
        QString requestType;
        bool ok = false;
        QString reasonCode;
        QString comment;
    };

    explicit ObsClient(ObsTransport &transport, ClientTiming timing = {},
                       QObject *parent = nullptr);
    ~ObsClient() override;

    ObsClient(const ObsClient &) = delete;
    ObsClient &operator=(const ObsClient &) = delete;

    // `url` is `ws://host:port`; `password` may be empty for an OBS whose
    // server authentication is off. Calling start() again with new
    // credentials reconnects with them.
    void start(const QString &url, const QString &password);
    // Stops and cancels every pending request. A reply that arrives after
    // this is dropped, never delivered against the next connection.
    void stop();

    [[nodiscard]] const ObsSnapshot &snapshot() const noexcept {
        return m_snapshot;
    }
    [[nodiscard]] ConnectionState state() const noexcept {
        return m_snapshot.state;
    }
    [[nodiscard]] QString reasonCode() const { return m_snapshot.reasonCode; }

    // Typed operations. Each returns the request id that will appear in
    // `operationFinished`, or 0 when the client is not Ready — a caller must
    // be able to tell "sent" from "not sent" without guessing.
    quint64 setOutputActive(OutputKind kind, bool active);
    quint64 setCurrentProgramScene(const QString &sceneName);
    quint64 setInputMuted(const QString &inputName, bool muted);
    // Re-reads everything the snapshot holds. The client does this itself on
    // identify; a surface needs it only after it repairs OBS's configuration.
    quint64 refresh();
    // Asks the QindaQt OBS bridge for the console mapping. Answers with 0
    // when not Ready; a bridge that is not loaded answers with a failed
    // result, which is how the route tells "no plugin" from "no console".
    quint64 refreshConsoleMapping();

Q_SIGNALS:
    void snapshotChanged();
    void stateChanged(QindaQt::Obs::ConnectionState state,
                      const QString &reasonCode);
    void operationFinished(
        const QindaQt::Obs::ObsClient::OperationResult &result);

private Q_SLOTS:
    void handleConnected();
    void handleDisconnected(const QString &reason);
    void handleText(const QString &text);
    void handleTransportError(const QString &reason);
    void handleRequestTimeouts();
    void handleStatisticsTick();

private:
    struct Pending {
        quint64 id = 0;
        QString requestType;
        qint64 deadlineMs = 0;
        // Set for the internal reads the client issues itself, so a surface
        // is not told about work it never asked for.
        bool internal = false;
    };

    void publish(ConnectionState state, const QString &reasonCode);
    void scheduleReconnect();
    void cancelPending(const QString &reasonCode);
    [[nodiscard]] QString nextRequestId(const QString &requestType,
                                        bool internal);
    quint64 send(const QString &requestType, const QJsonObject &data,
                 bool internal);
    void applyResponse(const Protocol::RequestResponse &response);
    void applyEvent(const Protocol::Event &event);
    void requestInitialState();
    void updateStatisticsTimer();

    ObsTransport &m_transport;
    ClientTiming m_timing;
    QTimer m_reconnectTimer;
    QTimer m_timeoutTimer;
    QTimer m_statisticsTimer;
    QString m_url;
    QString m_password;
    ObsSnapshot m_snapshot;
    QHash<QString, Pending> m_pending;
    quint64 m_nextRequestId = 1;
    qsizetype m_reconnectIndex = 0;
    bool m_started = false;
    // True once an Identified frame has landed on this connection, so a
    // second Hello on the same socket is treated as a fault.
    bool m_identified = false;
};

} // namespace QindaQt::Obs

Q_DECLARE_METATYPE(QindaQt::Obs::ObsClient::OperationResult)

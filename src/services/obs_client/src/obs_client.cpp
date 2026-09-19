// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_client.h>

#include <qindaqt/services/obs_client/obs_protocol.h>

#include <QDateTime>
#include <QJsonObject>

namespace QindaQt::Obs {

using namespace QindaQt::Obs::Protocol;

namespace {

constexpr int TimeoutSweepMilliseconds = 250;
// A surface that has lost its OBS should not accumulate work; the client
// refuses to hold more than this many requests at once.
constexpr int MaxPending = 64;
constexpr char VendorName[] = "qindaqt";
constexpr char MappingRequest[] = "GetConsoleMapping";
constexpr char MappingChangedEvent[] = "ConsoleMappingChanged";

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

} // namespace

bool ClientTiming::isValid() const {
    if (reconnectMilliseconds.isEmpty() || requestTimeoutMilliseconds <= 0 ||
        statisticsIntervalMilliseconds <= 0) {
        return false;
    }
    for (const int delay : reconnectMilliseconds) {
        if (delay <= 0) {
            return false;
        }
    }
    return true;
}

ObsClient::ObsClient(ObsTransport &transport, ClientTiming timing,
                     QObject *parent)
    : QObject(parent), m_transport(transport),
      m_timing(timing.isValid() ? std::move(timing) : ClientTiming{}) {
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (m_started) {
            m_transport.open(m_url);
        }
    });
    m_timeoutTimer.setInterval(TimeoutSweepMilliseconds);
    connect(&m_timeoutTimer, &QTimer::timeout, this,
            &ObsClient::handleRequestTimeouts);
    m_statisticsTimer.setInterval(m_timing.statisticsIntervalMilliseconds);
    connect(&m_statisticsTimer, &QTimer::timeout, this,
            &ObsClient::handleStatisticsTick);

    connect(&m_transport, &ObsTransport::connected, this,
            &ObsClient::handleConnected);
    connect(&m_transport, &ObsTransport::disconnected, this,
            &ObsClient::handleDisconnected);
    connect(&m_transport, &ObsTransport::textReceived, this,
            &ObsClient::handleText);
    connect(&m_transport, &ObsTransport::errorOccurred, this,
            &ObsClient::handleTransportError);
}

ObsClient::~ObsClient() = default;

void ObsClient::start(const QString &url, const QString &password) {
    const bool replacingConnection = m_started || m_transport.isOpen();
    m_started = false;
    m_identified = false;
    m_reconnectTimer.stop();
    m_statisticsTimer.stop();
    cancelPending(QStringLiteral("obs-connection-replaced"));
    // Close an in-progress connect too: isOpen() covers only an established
    // socket. Neither pending replies nor live outputs belong to the new one.
    if (replacingConnection) {
        m_transport.close();
    }
    m_snapshot = ObsSnapshot{};
    m_url = url;
    m_password = password;
    m_started = true;
    m_reconnectIndex = 0;
    m_reconnectTimer.stop();
    m_timeoutTimer.start();
    publish(ConnectionState::Connecting, QString::fromLatin1(ReasonCodes::None));
    m_transport.open(m_url);
}

void ObsClient::stop() {
    m_started = false;
    m_reconnectTimer.stop();
    m_timeoutTimer.stop();
    m_statisticsTimer.stop();
    m_identified = false;
    cancelPending(QStringLiteral("obs-client-stopped"));
    m_transport.close();
    m_snapshot = ObsSnapshot{};
    publish(ConnectionState::Disconnected,
            QString::fromLatin1(ReasonCodes::NotRunning));
}

void ObsClient::updateStatisticsTimer() {
    // AGENT-GUARD: only while something is actually running. A timer that
    // ran against an idle OBS would wake the shell forever for numbers
    // nobody is looking at.
    const bool wanted = m_snapshot.state == ConnectionState::Ready &&
                        (m_snapshot.record.active || m_snapshot.stream.active);
    if (wanted && !m_statisticsTimer.isActive()) {
        m_statisticsTimer.start();
    } else if (!wanted && m_statisticsTimer.isActive()) {
        m_statisticsTimer.stop();
    }
}

void ObsClient::handleStatisticsTick() {
    if (m_snapshot.record.active) {
        send(QString::fromLatin1(Requests::GetRecordStatus), {}, true);
    }
    if (m_snapshot.stream.active) {
        send(QString::fromLatin1(Requests::GetStreamStatus), {}, true);
    }
}

void ObsClient::publish(ConnectionState state, const QString &reasonCode) {
    const bool stateMoved =
        m_snapshot.state != state || m_snapshot.reasonCode != reasonCode;
    m_snapshot.state = state;
    m_snapshot.reasonCode = reasonCode;
    updateStatisticsTimer();
    if (stateMoved) {
        Q_EMIT stateChanged(state, reasonCode);
    }
    Q_EMIT snapshotChanged();
}

void ObsClient::scheduleReconnect() {
    if (!m_started) {
        return;
    }
    const qsizetype index =
        qMin(m_reconnectIndex, m_timing.reconnectMilliseconds.size() - 1);
    m_reconnectTimer.start(m_timing.reconnectMilliseconds.at(index));
    if (m_reconnectIndex < m_timing.reconnectMilliseconds.size() - 1) {
        ++m_reconnectIndex;
    }
}

void ObsClient::cancelPending(const QString &reasonCode) {
    const QHash<QString, Pending> pending = std::move(m_pending);
    m_pending.clear();
    for (const Pending &entry : pending) {
        if (entry.internal) {
            continue;
        }
        Q_EMIT operationFinished(OperationResult{entry.id, entry.requestType,
                                                 false, reasonCode, {}});
    }
}

void ObsClient::handleConnected() {
    // Connected is not usable yet: OBS sends Hello and the client identifies.
    publish(ConnectionState::Authenticating,
            QString::fromLatin1(ReasonCodes::None));
}

void ObsClient::handleDisconnected(const QString &reason) {
    Q_UNUSED(reason)
    m_identified = false;
    cancelPending(QStringLiteral("obs-connection-lost"));
    // AGENT-GUARD: A lost connection clears the live state. Leaving the last
    // known scene and output status published would let the top bar show a
    // recording that stopped when OBS quit.
    const ConnectionState previous = m_snapshot.state;
    const QString previousReason = m_snapshot.reasonCode;
    m_snapshot = ObsSnapshot{};
    // A refusal that closed the socket keeps its own reason; an ordinary
    // close means OBS is not there.
    // obs-websocket defines 4009/4010; the close text is not a stable API.
    const int closeCode = m_transport.closeCode();
    const QString refusal = closeCode == 4009
        ? QString::fromLatin1(ReasonCodes::AuthRejected)
        : closeCode == 4010 ? QString::fromLatin1(ReasonCodes::RpcVersion)
                           : previous == ConnectionState::Degraded
                                 ? previousReason : QString{};
    const bool refused = m_started && !refusal.isEmpty()
        && refusal != QString::fromLatin1(ReasonCodes::Closed);
    publish(m_started ? (refused ? ConnectionState::Degraded
                                 : ConnectionState::Connecting)
                      : ConnectionState::Disconnected,
            refused ? refusal : QString::fromLatin1(ReasonCodes::NotRunning));
    scheduleReconnect();
}

void ObsClient::handleTransportError(const QString &reason) {
    Q_UNUSED(reason)
    // The transport reports the close separately; an error alone must not
    // move the state, or a transient socket warning would blank the top bar.
}

void ObsClient::handleText(const QString &text) {
    if (!m_transport.isOpen()) {
        return;
    }
    const auto frame = decodeFrame(text);
    if (!frame.has_value()) {
        publish(ConnectionState::Degraded,
                QString::fromLatin1(ReasonCodes::Malformed));
        m_transport.close();
        return;
    }
    switch (frame->op) {
    case OpCode::Hello: {
        if (m_identified) {
            // A second Hello on an identified socket is not something this
            // protocol does.
            publish(ConnectionState::Degraded,
                    QString::fromLatin1(ReasonCodes::Malformed));
            m_transport.close();
            return;
        }
        if (frame->hello.rpcVersion < RpcVersion) {
            publish(ConnectionState::Degraded,
                    QString::fromLatin1(ReasonCodes::RpcVersion));
            m_transport.close();
            return;
        }
        if (frame->hello.requiresAuthentication() && m_password.isEmpty()) {
            // Identify anyway: OBS's own refusal is the truth, and telling
            // the user "OBS wants a password" is more useful than a silent
            // reconnect loop.
            publish(ConnectionState::Authenticating,
                    QString::fromLatin1(ReasonCodes::AuthRequired));
        }
        m_transport.sendText(encodeIdentify(frame->hello, m_password));
        return;
    }
    case OpCode::Identified: {
        if (m_identified) {
            publish(ConnectionState::Degraded,
                    QString::fromLatin1(ReasonCodes::Malformed));
            m_transport.close();
            return;
        }
        if (frame->negotiatedRpcVersion != RpcVersion) {
            publish(ConnectionState::Degraded,
                    QString::fromLatin1(ReasonCodes::RpcVersion));
            m_transport.close();
            return;
        }
        m_identified = true;
        m_reconnectIndex = 0;
        publish(ConnectionState::Ready,
                QString::fromLatin1(ReasonCodes::None));
        requestInitialState();
        return;
    }
    case OpCode::RequestResponse:
        applyResponse(frame->response);
        return;
    case OpCode::Event:
        applyEvent(frame->event);
        return;
    case OpCode::Identify:
    case OpCode::Reidentify:
    case OpCode::Request:
        // Frames only a client sends; a server that sends one is broken.
        publish(ConnectionState::Degraded,
                QString::fromLatin1(ReasonCodes::Malformed));
        m_transport.close();
        return;
    }
}

QString ObsClient::nextRequestId(const QString &requestType, bool internal) {
    const quint64 id = m_nextRequestId++;
    const QString key = QStringLiteral("qindaqt-%1").arg(id);
    m_pending.insert(key,
                     Pending{id, requestType,
                             nowMs() + m_timing.requestTimeoutMilliseconds,
                             internal});
    return key;
}

quint64 ObsClient::send(const QString &requestType, const QJsonObject &data,
                        bool internal) {
    if (m_snapshot.state != ConnectionState::Ready ||
        m_pending.size() >= MaxPending) {
        return 0;
    }
    const QString key = nextRequestId(requestType, internal);
    const quint64 id = m_pending.value(key).id;
    if (requestType == QLatin1String(Requests::CallVendorRequest)) {
        m_transport.sendText(encodeVendorRequest(
            QString::fromLatin1(VendorName),
            QString::fromLatin1(MappingRequest), key));
    } else {
        m_transport.sendText(encodeRequest(requestType, key, data));
    }
    return id;
}

void ObsClient::requestInitialState() {
    // One read of everything the snapshot holds; from here OBS's events keep
    // it current, so nothing polls.
    send(QString::fromLatin1(Requests::GetVersion), {}, true);
    send(QString::fromLatin1(Requests::GetRecordStatus), {}, true);
    send(QString::fromLatin1(Requests::GetStreamStatus), {}, true);
    send(QString::fromLatin1(Requests::GetVirtualCamStatus), {}, true);
    send(QString::fromLatin1(Requests::GetSceneList), {}, true);
    send(QString::fromLatin1(Requests::GetInputList), {}, true);
    send(QString::fromLatin1(Requests::CallVendorRequest), {}, true);
}

quint64 ObsClient::refresh() {
    if (m_snapshot.state != ConnectionState::Ready) {
        return 0;
    }
    requestInitialState();
    return send(QString::fromLatin1(Requests::GetVersion), {}, false);
}

quint64 ObsClient::refreshConsoleMapping() {
    return send(QString::fromLatin1(Requests::CallVendorRequest), {}, false);
}

quint64 ObsClient::setOutputActive(OutputKind kind, bool active) {
    const char *request = nullptr;
    switch (kind) {
    case OutputKind::Record:
        request = active ? Requests::StartRecord : Requests::StopRecord;
        break;
    case OutputKind::Stream:
        request = active ? Requests::StartStream : Requests::StopStream;
        break;
    case OutputKind::VirtualCam:
        request = active ? Requests::StartVirtualCam : Requests::StopVirtualCam;
        break;
    }
    return send(QString::fromLatin1(request), {}, false);
}

quint64 ObsClient::setCurrentProgramScene(const QString &sceneName) {
    if (sceneName.isEmpty()) {
        return 0;
    }
    return send(QString::fromLatin1(Requests::SetCurrentProgramScene),
                QJsonObject{{QStringLiteral("sceneName"), sceneName}}, false);
}

quint64 ObsClient::setInputMuted(const QString &inputName, bool muted) {
    if (inputName.isEmpty()) {
        return 0;
    }
    return send(QString::fromLatin1(Requests::SetInputMute),
                QJsonObject{{QStringLiteral("inputName"), inputName},
                            {QStringLiteral("inputMuted"), muted}},
                false);
}

void ObsClient::applyResponse(const RequestResponse &response) {
    const auto it = m_pending.constFind(response.requestId);
    if (it == m_pending.constEnd()) {
        // A reply to a request this connection did not make — a late frame
        // from a previous socket, or a server fault. Neither is ours.
        return;
    }
    const Pending pending = it.value();
    m_pending.erase(it);

    if (response.ok) {
        const QString &type = pending.requestType;
        bool moved = true;
        if (type == QLatin1String(Requests::GetVersion)) {
            m_snapshot.obsVersion =
                response.data.value(QStringLiteral("obsVersion")).toString();
            m_snapshot.webSocketVersion =
                response.data.value(QStringLiteral("obsWebSocketVersion"))
                    .toString();
        } else if (type == QLatin1String(Requests::GetRecordStatus)) {
            m_snapshot.record = recordStatusFrom(response.data);
        } else if (type == QLatin1String(Requests::GetStreamStatus)) {
            m_snapshot.stream = streamStatusFrom(response.data);
        } else if (type == QLatin1String(Requests::GetVirtualCamStatus)) {
            m_snapshot.virtualCam = virtualCamStatusFrom(response.data);
        } else if (type == QLatin1String(Requests::GetSceneList)) {
            m_snapshot.scenes = sceneListFrom(response.data);
        } else if (type == QLatin1String(Requests::GetInputList)) {
            m_snapshot.audioInputs = audioInputsFrom(response.data);
        } else if (type == QLatin1String(Requests::CallVendorRequest)) {
            const auto payload = unwrapVendorPayload(
                response.data, QString::fromLatin1(VendorName),
                QString::fromLatin1(MappingRequest));
            m_snapshot.consoleMapping =
                payload.has_value() ? consoleMappingFrom(*payload)
                                    : ConsoleMapping{};
        } else {
            moved = false;
        }
        if (moved) {
            updateStatisticsTimer();
            Q_EMIT snapshotChanged();
        }
    }

    if (!pending.internal) {
        Q_EMIT operationFinished(OperationResult{
            pending.id, pending.requestType, response.ok,
            response.ok ? QString()
                        : QStringLiteral("obs-request-failed-%1")
                              .arg(response.code),
            response.comment});
    }
}

void ObsClient::applyEvent(const Event &event) {
    const QString &type = event.eventType;
    bool moved = true;
    if (type == QLatin1String(Events::RecordStateChanged)) {
        const OutputStatus next = recordStatusFrom(event.data);
        const QString state = event.data.value(QStringLiteral("outputState")).toString();
        const bool paused = state == QLatin1String("OBS_WEBSOCKET_OUTPUT_PAUSED");
        const bool active = next.active || paused
            || state == QLatin1String("OBS_WEBSOCKET_OUTPUT_STOPPING");
        if (active != m_snapshot.record.active) {
            m_snapshot.record = {};
        }
        m_snapshot.record.active = active;
        m_snapshot.record.paused = paused;
    } else if (type == QLatin1String(Events::StreamStateChanged)) {
        const QString state = event.data.value(QStringLiteral("outputState")).toString();
        const bool reconnecting = state == QLatin1String("OBS_WEBSOCKET_OUTPUT_RECONNECTING");
        const bool active = event.data.value(QStringLiteral("outputActive")).toBool()
            || reconnecting || state == QLatin1String("OBS_WEBSOCKET_OUTPUT_STOPPING");
        if (active != m_snapshot.stream.active) {
            m_snapshot.stream = {};
        }
        m_snapshot.stream.active = active;
        m_snapshot.stream.reconnecting = reconnecting;
    } else if (type == QLatin1String(Events::VirtualcamStateChanged)) {
        m_snapshot.virtualCam.active =
            event.data.value(QStringLiteral("outputActive")).toBool();
    } else if (type == QLatin1String(Events::CurrentProgramSceneChanged)) {
        m_snapshot.scenes.currentProgramScene =
            event.data.value(QStringLiteral("sceneName")).toString();
    } else if (type == QLatin1String(Events::SceneListChanged) ||
               type == QLatin1String(Events::SceneCreated) ||
               type == QLatin1String(Events::SceneRemoved) ||
               type == QLatin1String(Events::SceneNameChanged)) {
        // The event carries only the delta; one read keeps the order OBS
        // shows rather than reconstructing it from four event shapes.
        send(QString::fromLatin1(Requests::GetSceneList), {}, true);
        moved = false;
    } else if (type == QLatin1String(Events::InputMuteStateChanged)) {
        const QString name =
            event.data.value(QStringLiteral("inputName")).toString();
        const bool muted =
            event.data.value(QStringLiteral("inputMuted")).toBool();
        moved = false;
        for (AudioInput &input : m_snapshot.audioInputs) {
            if (input.name == name) {
                input.muted = muted;
                moved = true;
                break;
            }
        }
    } else if (type == QLatin1String(Events::InputCreated) ||
               type == QLatin1String(Events::InputRemoved)) {
        send(QString::fromLatin1(Requests::GetInputList), {}, true);
        moved = false;
    } else if (type == QLatin1String(Events::VendorEvent)) {
        const auto payload =
            unwrapVendorPayload(event.data, QString::fromLatin1(VendorName),
                                QString::fromLatin1(MappingChangedEvent));
        if (!payload.has_value()) {
            // Another plugin's vendor traffic. Not ours to read.
            return;
        }
        m_snapshot.consoleMapping = consoleMappingFrom(*payload);
    } else if (type == QLatin1String(Events::ExitStarted)) {
        // OBS said it is going away; say so now instead of after the socket
        // timeout, so the top bar glyph does not lie for a second.
        m_snapshot = ObsSnapshot{};
        publish(ConnectionState::Connecting,
                QString::fromLatin1(ReasonCodes::NotRunning));
        return;
    } else {
        moved = false;
    }
    if (moved) {
        updateStatisticsTimer();
        Q_EMIT snapshotChanged();
    }
}

void ObsClient::handleRequestTimeouts() {
    if (m_pending.isEmpty()) {
        return;
    }
    const qint64 now = nowMs();
    QStringList expired;
    for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
        if (it.value().deadlineMs <= now) {
            expired.append(it.key());
        }
    }
    for (const QString &key : expired) {
        const Pending pending = m_pending.take(key);
        if (pending.internal) {
            continue;
        }
        Q_EMIT operationFinished(OperationResult{pending.id,
                                                 pending.requestType, false,
                                                 QStringLiteral("obs-timeout"),
                                                 {}});
    }
}

} // namespace QindaQt::Obs

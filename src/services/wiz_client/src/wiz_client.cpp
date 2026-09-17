// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_client/wiz_client.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"
#include "qindaqt/services/wiz_protocol/wiz_messages.h"

namespace QindaQt::Wiz
{
namespace
{

// Discovery is a broadcast; it runs on start, then rarely, so an idle desktop
// is not a constant source of broadcast traffic on the user's network.
constexpr int discoveryIntervalMilliseconds = 60000;

// How many times capability interrogation is re-sent for one device before it
// is left alone until the next epoch. Three datagrams is enough for a lossy
// radio without turning an unresponsive peer into a retry loop.
constexpr int maximumInterrogations = 3;

} // namespace

SystemWizClock::SystemWizClock()
{
    m_timer.start();
}

quint64 SystemWizClock::monotonicMilliseconds() const
{
    return static_cast<quint64>(m_timer.elapsed());
}

WizClient::WizClient(WizTransport *transport, WizClock *clock, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_clock(clock)
{
    Q_ASSERT(m_transport != nullptr);
    Q_ASSERT(m_clock != nullptr);
    connect(m_transport, &WizTransport::datagramReceived, this,
            &WizClient::acceptDatagram);
    connect(m_transport, &WizTransport::transportFailed, this,
            &WizClient::acceptTransportFailure);
    m_ticker.setTimerType(Qt::CoarseTimer);
    connect(&m_ticker, &QTimer::timeout, this, &WizClient::tick);
}

WizClient::~WizClient() = default;

void WizClient::setPollIntervalMilliseconds(const int interval)
{
    m_pollInterval = qBound(500, interval, 600000);
    if (m_ticker.isActive()) {
        m_ticker.start(qMin(m_pollInterval, 1000));
    }
}

void WizClient::setRequestTimeoutMilliseconds(const int timeout)
{
    m_requestTimeout = qBound(200, timeout, 15000);
}

void WizClient::setAutomaticPolling(const bool automatic)
{
    m_automaticPolling = automatic;
    if (!automatic) {
        m_ticker.stop();
    } else if (m_state == ClientState::Ready || m_state == ClientState::Starting) {
        m_ticker.start(qMin(m_pollInterval, 1000));
    }
}

void WizClient::setState(const ClientState state, const QString &reasonCode)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged(state, reasonCode);
}

void WizClient::publishIfChanged(const bool changed)
{
    if (changed) {
        Q_EMIT snapshotChanged();
    }
}

void WizClient::start()
{
    if (m_state == ClientState::Ready || m_state == ClientState::Starting) {
        return;
    }
    m_model.start();
    setState(ClientState::Starting, QString());

    QString error;
    if (!m_transport->start(&error)) {
        publishIfChanged(m_model.setAvailability(
            Availability::Unavailable, QStringLiteral("transport-unavailable"), error));
        setState(ClientState::Unavailable, QStringLiteral("transport-unavailable"));
        return;
    }

    publishIfChanged(m_model.setAvailability(Availability::Ready, QString(), QString()));
    setState(ClientState::Ready, QString());
    if (m_automaticPolling) {
        m_ticker.start(qMin(m_pollInterval, 1000));
    }
    sendDiscovery();
    // A seeded device is polled at once so a known light is controllable
    // before the first broadcast answer arrives.
    for (const QString &mac : m_model.knownMacs()) {
        pollDevice(mac);
    }
}

void WizClient::stop()
{
    m_ticker.stop();
    // Nothing queued survives the loss of the transport; the caller is told so
    // rather than being left with intents that will never complete.
    const QList<PendingOperation> abandoned = m_queue;
    m_queue.clear();
    m_operationInFlight = false;
    for (const PendingOperation &operation : abandoned) {
        OperationResult result;
        result.kind = operation.kind;
        result.status = OperationStatus::Uncertain;
        result.targetMac = operation.mac;
        result.initiatingEpoch = operation.initiatingEpoch;
        result.initiatingRevision = operation.initiatingRevision;
        result.reasonCode = QStringLiteral("client-stopped");
        deliver(result, operation.requestId);
    }
    m_outstandingPolls.clear();
    m_interrogations.clear();
    m_transport->stop();
    m_model.stop();
    Q_EMIT snapshotChanged();
    setState(ClientState::Stopped, QStringLiteral("client-stopped"));
}

void WizClient::seedDevice(const QString &mac, const QString &address)
{
    const QString normalized = normalizeMac(mac);
    if (normalized.isEmpty() || address.isEmpty()) {
        return;
    }
    // A seeded endpoint is routing information only: the device becomes real
    // inventory when it answers with its own identity.
    DecodedMessage seed;
    seed.method = Method::GetPilot;
    seed.mac = normalized;
    publishIfChanged(m_model.observe(address, Limits::controlPort, seed));
    // Nothing has actually answered yet, so the device starts unreachable
    // rather than claiming to be online.
    publishIfChanged(m_model.noteMissedPoll(normalized));
    if (m_state == ClientState::Ready) {
        pollDevice(normalized);
    }
}

void WizClient::applyStoredLabel(const QString &mac, const QString &label)
{
    publishIfChanged(m_model.applyStoredLabel(mac, label));
}

void WizClient::forgetDevice(const QString &mac)
{
    const QString normalized = normalizeMac(mac);
    m_outstandingPolls.remove(normalized);
    m_interrogations.remove(normalized);
    publishIfChanged(m_model.forget(normalized));
}

bool WizClient::transmit(const QString &mac, const QByteArray &datagram)
{
    const auto endpoint = m_model.endpoint(mac);
    if (!endpoint.has_value()) {
        return false;
    }
    const quint16 port =
        endpoint->port != 0 ? endpoint->port : quint16{Limits::controlPort};
    return m_transport->send(endpoint->address, port, datagram);
}

void WizClient::sendDiscovery()
{
    m_lastDiscovery = m_clock->monotonicMilliseconds();
    publishIfChanged(m_model.setDiscovering(true));
    const QString listener = m_transport->listenerAddress();
    const QString listenerMac = m_transport->listenerHardwareAddress();
    // With a readable notification address the same datagram both discovers
    // lights and subscribes to their state pushes; without one it is only the
    // discovery ping.
    const QByteArray probe = listener.isEmpty()
        ? encodeRegistration(QStringLiteral("0.0.0.0"), QStringLiteral("000000000000"),
                             false)
        : encodeRegistration(listener, listenerMac, true);
    if (!m_transport->broadcast(Limits::controlPort, probe)) {
        publishIfChanged(m_model.setAvailability(
            Availability::Degraded, QStringLiteral("broadcast-failed"),
            QStringLiteral("Smart lights could not be searched for on this network.")));
    }
}

void WizClient::pollDevice(const QString &mac)
{
    if (!transmit(mac, encodeGetPilot())) {
        return;
    }
    if (!m_outstandingPolls.contains(mac)) {
        m_outstandingPolls.insert(mac, m_clock->monotonicMilliseconds());
    }
}

void WizClient::interrogate(const QString &mac)
{
    const int attempts = m_interrogations.value(mac, 0);
    if (attempts >= maximumInterrogations) {
        return;
    }
    m_interrogations.insert(mac, attempts + 1);
    // Identity first: the module name is what makes every other capability
    // answer meaningful. A send that cannot leave the host is not a fault of
    // this device; the next poll round tries again.
    if (!transmit(mac, encodeGetSystemConfig())) {
        return;
    }
    static_cast<void>(transmit(mac, encodeGetModelConfig()));
}

void WizClient::tick()
{
    if (m_state != ClientState::Ready) {
        return;
    }
    const quint64 now = m_clock->monotonicMilliseconds();

    if (m_operationInFlight && !m_queue.isEmpty() && now >= m_queue.first().deadline) {
        PendingOperation &current = m_queue.first();
        // One retry: a single lost datagram on a home Wi-Fi network is
        // ordinary, a second loss is a real fault.
        if (current.attempt < 2 && !current.datagram.isEmpty()) {
            ++current.attempt;
            current.deadline = now + static_cast<quint64>(m_requestTimeout);
            static_cast<void>(transmit(current.mac, current.datagram));
        } else {
            publishIfChanged(m_model.noteMissedPoll(current.mac));
            completeCurrent(OperationStatus::Uncertain, QStringLiteral("no-reply"),
                            QStringLiteral("The light did not confirm the change."));
        }
    }

    if (now - m_lastPollRound >= static_cast<quint64>(m_pollInterval)) {
        m_lastPollRound = now;
        const QStringList macs = m_model.knownMacs();
        for (const QString &mac : macs) {
            if (m_outstandingPolls.contains(mac)) {
                // The previous round went unanswered.
                m_outstandingPolls.remove(mac);
                publishIfChanged(m_model.noteMissedPoll(mac));
            }
            const auto device = m_model.device(mac);
            if (device.has_value() && !device->capabilitiesKnown) {
                interrogate(mac);
            }
            pollDevice(mac);
        }
        publishIfChanged(m_model.setDiscovering(false));
    }

    const bool nothingFound = m_model.knownMacs().isEmpty();
    const quint64 discoveryInterval = nothingFound
        ? static_cast<quint64>(m_pollInterval)
        : static_cast<quint64>(discoveryIntervalMilliseconds);
    if (now - m_lastDiscovery >= discoveryInterval) {
        sendDiscovery();
    }
}

void WizClient::acceptDatagram(const QString &address, const quint16 port,
                               const QByteArray &datagram)
{
    const auto message = decodeMessage(datagram);
    if (!message.has_value()) {
        return;
    }

    // AGENT-GUARD (verified on firmware 1.38.0): a getModelConfig reply
    // carries no `mac`, so the model is always given the datagram and decides
    // attribution itself. Filtering on a MAC here left capability truth
    // permanently incomplete and every capability-dependent control inert.
    QString mac = normalizeMac(message->mac);
    if (mac.isEmpty()) {
        mac = m_model.deviceAtAddress(address);
    }
    const auto before = mac.isEmpty() ? std::nullopt : m_model.device(mac);
    publishIfChanged(m_model.observe(address, port, *message));

    if (!mac.isEmpty()) {
        m_outstandingPolls.remove(mac);
        // A light that has just come back deserves a fresh interrogation
        // budget. Without this, a luminaire that was still joining the network
        // during its first rounds would stay capability-less until the shell
        // restarts, leaving its controls inert forever.
        if (before.has_value() && before->reachability != Reachability::Online) {
            m_interrogations.remove(mac);
        }
        const auto device = m_model.device(mac);
        if (device.has_value() && !device->capabilitiesKnown) {
            interrogate(mac);
        }
    }

    if (m_operationInFlight && !m_queue.isEmpty()) {
        const PendingOperation &current = m_queue.first();
        const bool sameDevice = (!mac.isEmpty() && mac == current.mac)
            || (mac.isEmpty() && address == current.address);
        if (sameDevice) {
            if (message->method == Method::SetPilot) {
                if (message->hasError) {
                    completeCurrent(OperationStatus::Failed,
                                    QStringLiteral("device-error"),
                                    message->errorMessage);
                } else if (message->acknowledged) {
                    // Ask for the new truth at once so the projection shows
                    // what the light actually settled on, not what was asked.
                    static_cast<void>(transmit(current.mac, encodeGetPilot()));
                    completeCurrent(OperationStatus::Succeeded, QString());
                } else {
                    completeCurrent(OperationStatus::Failed,
                                    QStringLiteral("not-acknowledged"), QString());
                }
            } else if (current.kind == OperationKind::Refresh && message->pilotKnown) {
                completeCurrent(OperationStatus::Succeeded, QString());
            }
        }
    }
}

void WizClient::acceptTransportFailure(const QString &reason)
{
    publishIfChanged(m_model.setAvailability(Availability::Unavailable,
                                             QStringLiteral("transport-failed"), reason));
    setState(ClientState::Unavailable, QStringLiteral("transport-failed"));
    m_ticker.stop();
    while (!m_queue.isEmpty()) {
        completeCurrent(OperationStatus::Uncertain, QStringLiteral("transport-failed"),
                        reason);
    }
}

} // namespace QindaQt::Wiz

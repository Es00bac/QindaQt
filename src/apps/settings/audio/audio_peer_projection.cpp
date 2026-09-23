// SPDX-License-Identifier: LGPL-3.0-or-later

// The manual-peer Settings projection (ADR-0246). Audio1 owns validation,
// persistence and the graph; this file offers bounded choices from its public
// snapshot and dispatches typed requests through the same AudioClient fence.

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <qindaqt/services/audio_protocol/audio_validation.h>

namespace QindaQt::Apps::SettingsAudio
{

using namespace QindaQt::Audio;

bool AudioSettingsModel::canManagePeerStreams() const
{
    return consoleAvailable()
        && snapshotAdmitsOperation(m_client, Capability::ManageVbanStreams);
}

QVariantList AudioSettingsModel::peerOutputs() const
{
    QVariantList rows;
    if (!m_client.hasSnapshot()) return rows;
    for (const Device &device : m_client.snapshot().outputs) {
        if (!isPhysicalPeerOutput(device)) continue;
        rows.append(QVariantMap{{QStringLiteral("nodeName"), device.nodeName},
                                {QStringLiteral("label"), device.description.isEmpty()
                                     ? device.name : device.description},
                                {QStringLiteral("serial"), device.handle.serial}});
    }
    return rows;
}

QVariantList AudioSettingsModel::peerBuses() const
{
    QVariantList rows;
    if (!m_client.hasSnapshot()) return rows;
    for (const Bus &bus : m_client.snapshot().console.buses) {
        rows.append(QVariantMap{{QStringLiteral("id"), bus.id},
                                {QStringLiteral("label"), bus.label},
                                {QStringLiteral("available"), bus.targetKnown}});
    }
    return rows;
}

bool AudioSettingsModel::saveOutgoingPeer(QString name, QString busId,
                                           QString destinationHost, const int port)
{
    if (!canManagePeerStreams()) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    bool knownBus = false;
    for (const Bus &bus : m_client.snapshot().console.buses)
        knownBus = knownBus || bus.id == busId;
    if (!knownBus) {
        rejectAction(QStringLiteral("unknown-bus"));
        return false;
    }
    VbanStream stream;
    stream.name = name.trimmed();
    stream.outgoing = true;
    stream.busId = busId;
    stream.host = destinationHost.trimmed();
    stream.port = port > 0 && port <= 65535 ? static_cast<quint32>(port) : 0;
    if (!validateVbanDefinition(stream).accepted) {
        rejectAction(QStringLiteral("invalid-vban-stream"));
        return false;
    }
    return trackConsoleRequest(m_client.upsertVbanStream(stream));
}

bool AudioSettingsModel::saveIncomingPeer(QString name, QString sourceIpv4,
                                           QString outputNodeName, const int port)
{
    if (!canManagePeerStreams()) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    bool physical = false;
    for (const Device &device : m_client.snapshot().outputs)
        physical = physical || (device.nodeName == outputNodeName
                                && isPhysicalPeerOutput(device));
    if (!physical) {
        rejectAction(QStringLiteral("invalid-target"));
        return false;
    }
    VbanStream stream;
    stream.name = name.trimmed();
    stream.outgoing = false;
    stream.host = sourceIpv4.trimmed();
    stream.port = port > 0 && port <= 65535 ? static_cast<quint32>(port) : 0;
    stream.outputNodeName = outputNodeName;
    if (!validateVbanDefinition(stream).accepted) {
        rejectAction(QStringLiteral("invalid-vban-stream"));
        return false;
    }
    return trackConsoleRequest(m_client.upsertVbanStream(stream));
}

bool AudioSettingsModel::removePeer(QString name)
{
    if (!canManagePeerStreams()) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    name = name.trimmed();
    bool known = false;
    for (const VbanStream &stream : m_client.snapshot().console.vban)
        known = known || stream.name == name;
    if (!known) {
        rejectAction(QStringLiteral("unknown-vban-stream"));
        return false;
    }
    return trackConsoleRequest(m_client.deleteVbanStream(name));
}

} // namespace QindaQt::Apps::SettingsAudio

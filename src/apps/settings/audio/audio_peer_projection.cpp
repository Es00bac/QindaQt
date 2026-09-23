// SPDX-License-Identifier: LGPL-3.0-or-later

// The manual-peer Settings projection (ADR-0246). Audio1 owns validation,
// persistence and the graph; this file offers bounded choices from its public
// snapshot and dispatches typed requests through the same AudioClient fence.

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include "audio_peer_code.h"
#include "audio_peer_addresses.h"

#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

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

QVariantList AudioSettingsModel::localPeerAddresses() const
{
    QVariantList rows;
    for (const QString &address : localPeerIpv4Addresses())
        rows.append(QVariantMap{{QStringLiteral("address"), address},
                                {QStringLiteral("label"), address}});
    return rows;
}

QVariantMap AudioSettingsModel::sharePeerCode(QString savedOutgoingName,
                                              QString thisComputerIpv4) const
{
    if (!m_client.hasSnapshot())
        return {{QStringLiteral("valid"), false},
                {QStringLiteral("reason"), QStringLiteral("unavailable")}};
    savedOutgoingName = savedOutgoingName.trimmed();
    thisComputerIpv4 = thisComputerIpv4.trimmed();
    for (const VbanStream &stream : m_client.snapshot().console.vban) {
        if (!stream.outgoing || stream.name != savedOutgoingName) continue;
        const QString code = encodePeerCode({stream.name, thisComputerIpv4, stream.port});
        if (!code.isEmpty()) {
            // AGENT-GUARD: this is UI-only identity, not part of the shared code.
            // Exact owner/epoch and saved sender configuration must still match
            // when the user copies; a snapshot revision alone is too volatile.
            const QJsonArray identity{serviceOwner(),
                                      QString::number(serviceEpoch()),
                                      stream.name, stream.busId, stream.host,
                                      static_cast<int>(stream.port)};
            const QString fingerprint = QString::fromUtf8(
                QJsonDocument(identity).toJson(QJsonDocument::Compact));
            return {{QStringLiteral("valid"), true},
                    {QStringLiteral("code"), code},
                    {QStringLiteral("name"), stream.name},
                    {QStringLiteral("sourceIpv4"), thisComputerIpv4},
                    {QStringLiteral("port"), stream.port},
                    {QStringLiteral("fingerprint"), fingerprint}};
        }
        break;
    }
    return {{QStringLiteral("valid"), false},
            {QStringLiteral("reason"), QStringLiteral("invalid-sender")}};
}

QVariantMap AudioSettingsModel::reviewPeerCode(QString code) const
{
    PeerCode peer;
    QString reason;
    if (!decodePeerCode(code.trimmed(), &peer, &reason))
        return {{QStringLiteral("valid"), false},
                {QStringLiteral("reason"), reason}};
    if (!m_client.hasSnapshot())
        return {{QStringLiteral("valid"), false},
                {QStringLiteral("reason"), QStringLiteral("unavailable")}};
    // AGENT-GUARD: a pasted code cannot silently replace another grant or
    // contend for an existing receive socket; recheck at Save after review.
    for (const VbanStream &stream : m_client.snapshot().console.vban) {
        if (stream.name == peer.name)
            return {{QStringLiteral("valid"), false},
                    {QStringLiteral("reason"), stream.outgoing
                         ? QStringLiteral("name-conflict")
                         : (stream.host == peer.sourceIpv4 && stream.port == peer.port
                                ? QStringLiteral("duplicate-peer")
                                : QStringLiteral("name-conflict"))}};
        if (!stream.outgoing && stream.port == peer.port)
            return {{QStringLiteral("valid"), false},
                    {QStringLiteral("reason"), QStringLiteral("port-conflict")}};
    }
    return {{QStringLiteral("valid"), true},
            {QStringLiteral("name"), peer.name},
            {QStringLiteral("sourceIpv4"), peer.sourceIpv4},
            {QStringLiteral("port"), peer.port}};
}

bool AudioSettingsModel::saveImportedPeer(QString code, QString outputNodeName)
{
    const QVariantMap review = reviewPeerCode(code);
    if (!review.value(QStringLiteral("valid")).toBool()) {
        rejectAction(review.value(QStringLiteral("reason")).toString());
        return false;
    }
    return saveIncomingPeer(review.value(QStringLiteral("name")).toString(),
                            review.value(QStringLiteral("sourceIpv4")).toString(),
                            outputNodeName,
                            review.value(QStringLiteral("port")).toInt());
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

// SPDX-License-Identifier: LGPL-3.0-or-later

// The console half of AudioClient (ADR-0173 onward): every intent that
// addresses a strip, a bus or a preset. Split from audio_client.cpp so the
// client's lifecycle and its console surface read separately and each file
// stays inside its source-shape budget.

#include <qindaqt/services/audio_client/audio_client.h>

namespace QindaQt::Audio
{
namespace {
[[nodiscard]] OperationRequest consoleRequest(OperationKind kind, const QString &id)
{
    OperationRequest request;
    request.kind = kind;
    request.consoleId = id;
    return request;
}

} // namespace

quint64 AudioClient::setStripGain(const QString &stripId, const double gainDb)
{
    auto request = consoleRequest(OperationKind::SetStripGain, stripId);
    request.gainDb = gainDb;
    return beginOperation(request);
}

quint64 AudioClient::setStripMuted(const QString &stripId, const bool muted)
{
    auto request = consoleRequest(OperationKind::SetStripMute, stripId);
    request.muted = muted;
    return beginOperation(request);
}

quint64 AudioClient::setStripSoloed(const QString &stripId, const bool soloed)
{
    auto request = consoleRequest(OperationKind::SetStripSolo, stripId);
    request.enabled = soloed;
    return beginOperation(request);
}

quint64 AudioClient::setStripMono(const QString &stripId, const bool mono)
{
    auto request = consoleRequest(OperationKind::SetStripMono, stripId);
    request.enabled = mono;
    return beginOperation(request);
}

quint64 AudioClient::setStripPan(const QString &stripId, const double pan)
{
    auto request = consoleRequest(OperationKind::SetStripPan, stripId);
    request.pan = pan;
    return beginOperation(request);
}

quint64 AudioClient::setStripTrim(const QString &stripId,
                                  const QVector<double> &trimDb)
{
    auto request = consoleRequest(OperationKind::SetStripTrim, stripId);
    request.channelVolumes = trimDb;
    return beginOperation(request);
}

quint64 AudioClient::setStripSend(const QString &stripId, const quint32 busIndex,
                                  const bool enabled, const double gainDb)
{
    auto request = consoleRequest(OperationKind::SetStripSend, stripId);
    request.busIndex = busIndex;
    request.enabled = enabled;
    request.gainDb = gainDb;
    return beginOperation(request);
}

quint64 AudioClient::setBusGain(const QString &busId, const double gainDb)
{
    auto request = consoleRequest(OperationKind::SetBusGain, busId);
    request.gainDb = gainDb;
    return beginOperation(request);
}

quint64 AudioClient::setBusMuted(const QString &busId, const bool muted)
{
    auto request = consoleRequest(OperationKind::SetBusMute, busId);
    request.muted = muted;
    return beginOperation(request);
}

quint64 AudioClient::setBusMono(const QString &busId, const bool mono)
{
    auto request = consoleRequest(OperationKind::SetBusMono, busId);
    request.enabled = mono;
    return beginOperation(request);
}

quint64 AudioClient::setBusTarget(const QString &busId, const Handle &device)
{
    auto request = consoleRequest(OperationKind::SetBusTarget, busId);
    request.primary = device;
    return beginOperation(request);
}

quint64 AudioClient::setStripSource(const QString &stripId, const Handle &device)
{
    auto request = consoleRequest(OperationKind::SetStripSource, stripId);
    request.primary = device;
    return beginOperation(request);
}

quint64 AudioClient::setStripProcessing(const QString &stripId,
                                        const StripProcessing &processing)
{
    auto request = consoleRequest(OperationKind::SetStripProcessing, stripId);
    request.processing = processing;
    return beginOperation(request);
}

quint64 AudioClient::setBusProcessing(const QString &busId, const BusProcessing &processing)
{
    auto request = consoleRequest(OperationKind::SetBusProcessing, busId);
    request.busProcessing = processing;
    return beginOperation(request);
}

quint64 AudioClient::savePreset(const QString &name)
{
    OperationRequest request;
    request.kind = OperationKind::SavePreset;
    request.displayName = name;
    return beginOperation(request);
}

quint64 AudioClient::loadPreset(const QString &name)
{
    OperationRequest request;
    request.kind = OperationKind::LoadPreset;
    request.displayName = name;
    return beginOperation(request);
}

quint64 AudioClient::deletePreset(const QString &name)
{
    OperationRequest request;
    request.kind = OperationKind::DeletePreset;
    request.displayName = name;
    return beginOperation(request);
}

quint64 AudioClient::runMacro(const QString &name)
{
    OperationRequest request;
    request.kind = OperationKind::RunMacro;
    request.displayName = name;
    return beginOperation(request);
}

quint64 AudioClient::startRecording(const QString &busId, const QString &format)
{
    auto request = consoleRequest(OperationKind::StartRecording, busId);
    request.displayName = format;
    return beginOperation(request);
}

quint64 AudioClient::stopRecording()
{
    OperationRequest request;
    request.kind = OperationKind::StopRecording;
    return beginOperation(request);
}

quint64 AudioClient::setVbanEnabled(const QString &name, const bool enabled)
{
    OperationRequest request;
    request.kind = OperationKind::SetVbanEnabled;
    request.displayName = name;
    request.enabled = enabled;
    return beginOperation(request);
}

quint64 AudioClient::upsertVbanStream(const VbanStream &definition)
{
    OperationRequest request;
    request.kind = OperationKind::UpsertVbanStream;
    request.vbanDefinition = definition;
    return beginOperation(request);
}

quint64 AudioClient::deleteVbanStream(const QString &name)
{
    OperationRequest request;
    request.kind = OperationKind::DeleteVbanStream;
    request.displayName = name;
    return beginOperation(request);
}

} // namespace QindaQt::Audio

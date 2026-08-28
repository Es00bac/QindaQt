// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_applet_controller.h"

#include <QtCore/QSet>

namespace QindaQt::Shell::AudioApplet {

using Audio::AudioClient;
using Audio::ClientState;
using Audio::Device;
using Audio::OperationResult;
using Audio::OperationStatus;
using Audio::Snapshot;
using Audio::Stream;

namespace {

QString phaseKey(Phase phase)
{
    switch (phase) {
    case Phase::Ready:
        return QStringLiteral("ready");
    case Phase::Degraded:
        return QStringLiteral("degraded");
    case Phase::Unavailable:
        return QStringLiteral("unavailable");
    case Phase::Loading:
        break;
    }
    return QStringLiteral("loading");
}

QString stablePhaseReasonText(const QString &reasonCode)
{
    if (reasonCode == QLatin1String("unavailable"))
        return QObject::tr("The audio service is not available right now.");
    if (reasonCode == QLatin1String("malformed-snapshot")
        || reasonCode == QLatin1String("backend-malformed"))
        return QObject::tr(
            "Reported audio information could not be trusted, so it is hidden.");
    if (reasonCode == QLatin1String("pipewire-replaced")
        || reasonCode == QLatin1String("wireplumber-replaced"))
        return QObject::tr(
            "The audio system was replaced; the list is being refreshed.");
    if (reasonCode == QLatin1String("owner-replaced")
        || reasonCode == QLatin1String("authority-replaced"))
        return QObject::tr(
            "The audio service connection changed; the list is being refreshed.");
    return QObject::tr("Audio device information is limited right now.");
}

const Device *findDevice(const Snapshot &snapshot, quint64 serial)
{
    for (const Device &device : snapshot.outputs)
        if (device.handle.serial == serial)
            return &device;
    for (const Device &device : snapshot.inputs)
        if (device.handle.serial == serial)
            return &device;
    return nullptr;
}

const Stream *findStream(const Snapshot &snapshot, quint64 serial)
{
    for (const Stream &stream : snapshot.streams)
        if (stream.handle.serial == serial)
            return &stream;
    return nullptr;
}

} // namespace

AudioAppletController::AudioAppletController(AudioClient *client,
                                             QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    // AGENT-GUARD: The controller is presentation-only. It must never start,
    // stop, or parent the client; shell composition owns the client lifetime,
    // and taking ownership here would couple panel teardown to transport
    // teardown.
    Q_ASSERT(m_client != nullptr);

    connect(m_client, &AudioClient::stateChanged, this,
            &AudioAppletController::reproject);
    connect(m_client, &AudioClient::snapshotChanged, this,
            &AudioAppletController::reproject);
    connect(m_client, &AudioClient::operationCompleted, this,
            &AudioAppletController::handleOperationCompleted);

    reproject();
}

QString AudioAppletController::phaseText() const
{
    return phaseKey(m_model.phase());
}

QString AudioAppletController::phaseReasonText() const
{
    return stablePhaseReasonText(m_model.phaseReasonCode());
}

QVariantList AudioAppletController::deviceRows() const
{
    QVariantList rows;
    rows.reserve(m_model.deviceRows().size());
    for (const DeviceRow &row : m_model.deviceRows())
        rows.append(QVariant::fromValue(row));
    return rows;
}

QVariantList AudioAppletController::streamRows() const
{
    QVariantList rows;
    rows.reserve(m_model.streamRows().size());
    for (const StreamRow &row : m_model.streamRows())
        rows.append(QVariant::fromValue(row));
    return rows;
}

void AudioAppletController::clearFeedback()
{
    if (m_feedback.isEmpty())
        return;
    m_feedback.clear();
    Q_EMIT feedbackChanged();
}

void AudioAppletController::publishFeedback(const QString &message)
{
    if (message.isEmpty())
        return;
    m_feedback = message;
    Q_EMIT feedbackChanged();
}

bool AudioAppletController::requestVolume(quint64 serial, bool isStream,
                                          double volume)
{
    return beginRequest(serial, isStream, RequestKind::Volume, volume, false);
}

bool AudioAppletController::requestMute(quint64 serial, bool isStream,
                                        bool muted)
{
    return beginRequest(serial, isStream, RequestKind::Mute, 0.0, muted);
}

void AudioAppletController::prunePendingAgainstSnapshot()
{
    if (!m_client->hasSnapshot())
        return;

    // AGENT-NOTE: Serials vanish when the service replaces its epoch or the
    // object left the graph. The pending flags are presentation state only;
    // dropping them without feedback keeps a replaced lineage from pinning a
    // row disabled forever. Late completions then find no request ID and are
    // ignored, which matches the Audio1 no-replay contract.
    const Snapshot snapshot = m_client->snapshot();
    QSet<quint64> liveSerials;
    liveSerials.reserve(snapshot.outputs.size() + snapshot.inputs.size()
                        + snapshot.streams.size());
    for (const Device &device : snapshot.outputs)
        liveSerials.insert(device.handle.serial);
    for (const Device &device : snapshot.inputs)
        liveSerials.insert(device.handle.serial);
    for (const Stream &stream : snapshot.streams)
        liveSerials.insert(stream.handle.serial);

    auto serialIt = m_pendingBySerial.begin();
    while (serialIt != m_pendingBySerial.end()) {
        if (!liveSerials.contains(serialIt.key())) {
            m_serialByRequestId.remove(serialIt->requestId);
            serialIt = m_pendingBySerial.erase(serialIt);
        } else {
            ++serialIt;
        }
    }
}

void AudioAppletController::reproject()
{
    prunePendingAgainstSnapshot();

    QSet<quint64> pendingSerials;
    pendingSerials.reserve(m_pendingBySerial.size());
    for (auto it = m_pendingBySerial.constBegin();
         it != m_pendingBySerial.constEnd(); ++it)
        pendingSerials.insert(it.key());

    const ClientState clientState = m_client->state();
    const bool hasSnapshot = m_client->hasSnapshot();
    const Snapshot snapshot = hasSnapshot ? m_client->snapshot() : Snapshot{};

    Phase phase = Phase::Loading;
    QString reasonCode = m_client->reasonCode();
    switch (clientState) {
    case ClientState::Stopped:
    case ClientState::Starting:
        phase = Phase::Loading;
        break;
    case ClientState::Unavailable:
        phase = Phase::Unavailable;
        break;
    case ClientState::Degraded:
    case ClientState::Ready:
        if (!hasSnapshot) {
            phase = Phase::Loading;
            reasonCode.clear();
            break;
        }
        switch (snapshot.availability) {
        case Audio::Availability::Starting:
            phase = Phase::Loading;
            reasonCode.clear();
            break;
        case Audio::Availability::Ready:
            phase = Phase::Ready;
            reasonCode.clear();
            break;
        case Audio::Availability::Degraded:
            phase = Phase::Degraded;
            reasonCode = snapshot.reasonCode;
            break;
        case Audio::Availability::Unavailable:
            phase = Phase::Unavailable;
            reasonCode = snapshot.reasonCode;
            break;
        }
        break;
    }

    const Snapshot *projectedSnapshot = hasSnapshot ? &snapshot : nullptr;
    m_model = AudioAppletModel::project(phase, reasonCode, projectedSnapshot,
                                        pendingSerials);
    Q_EMIT stateReprojected();
}

bool AudioAppletController::beginRequest(quint64 serial, bool isStream,
                                         RequestKind kind, double volume,
                                         bool muted)
{
    // The presentation clamp runs before any capability lookup so that a
    // non-finite or out-of-range value can never become a dispatch. The
    // client validates again, but this controller never knowingly forwards a
    // domain-invalid level.
    double clampedVolume = 0.0;
    if (kind == RequestKind::Volume) {
        const std::optional<double> clamped =
            AudioAppletModel::clampVolumeLevel(volume);
        if (!clamped) {
            publishFeedback(
                QObject::tr("That volume value was not accepted."));
            return false;
        }
        clampedVolume = *clamped;
    }

    if (!m_client->hasSnapshot()) {
        publishFeedback(QObject::tr(
            "Audio information is not available, so the change was not sent."));
        return false;
    }
    const Snapshot snapshot = m_client->snapshot();

    if (m_pendingBySerial.contains(serial)) {
        publishFeedback(
            QObject::tr("A change for this item is already in progress."));
        return false;
    }

    quint64 dispatchedRequestId = 0;
    if (isStream) {
        const Stream *stream = findStream(snapshot, serial);
        if (stream == nullptr) {
            publishFeedback(
                QObject::tr("That application stream is no longer listed."));
            return false;
        }
        if (kind == RequestKind::Volume && !stream->canSetVolume) {
            publishFeedback(QObject::tr(
                "This application stream does not support volume changes."));
            return false;
        }
        if (kind == RequestKind::Mute && !stream->canSetMute) {
            publishFeedback(QObject::tr(
                "This application stream does not support mute changes."));
            return false;
        }
        if (kind == RequestKind::Volume)
            dispatchedRequestId = m_client->setVolume(stream->handle,
                                                      clampedVolume);
        else
            dispatchedRequestId = m_client->setMute(stream->handle, muted);
    } else {
        const Device *device = findDevice(snapshot, serial);
        if (device == nullptr) {
            publishFeedback(QObject::tr("That device is no longer listed."));
            return false;
        }
        if (kind == RequestKind::Volume && !device->canSetVolume) {
            publishFeedback(QObject::tr(
                "This device does not support volume changes."));
            return false;
        }
        if (kind == RequestKind::Mute && !device->canSetMute) {
            publishFeedback(
                QObject::tr("This device does not support mute changes."));
            return false;
        }
        if (kind == RequestKind::Volume)
            dispatchedRequestId = m_client->setVolume(device->handle,
                                                      clampedVolume);
        else
            dispatchedRequestId = m_client->setMute(device->handle, muted);
    }

    if (dispatchedRequestId == 0) {
        // The client only ever returns nonzero request IDs for accepted
        // dispatches; zero would mean it refused locally.
        publishFeedback(QObject::tr("The change could not be sent."));
        return false;
    }

    m_pendingBySerial.insert(serial, PendingRequest{dispatchedRequestId, kind});
    m_serialByRequestId.insert(dispatchedRequestId, serial);
    reproject();
    return true;
}

QString AudioAppletController::requestFailureText(
    const OperationResult &result, RequestKind kind) const
{
    const QString what =
        kind == RequestKind::Volume ? QObject::tr("volume") : QObject::tr("mute");
    switch (result.status) {
    case OperationStatus::Succeeded:
        return {};
    case OperationStatus::Uncertain:
        return QObject::tr("The %1 change could not be confirmed. Shown "
                           "state will refresh from the audio service.")
            .arg(what);
    case OperationStatus::Busy:
    case OperationStatus::Rejected:
    case OperationStatus::Unsupported:
    case OperationStatus::Failed:
        break;
    }

    // Branch on the stable reason code, never on diagnostic text; diagnostics
    // are for logs, not for user-facing decisions.
    const QString &reason = result.reasonCode;
    if (reason == QLatin1String("invalid-volume"))
        return QObject::tr("That volume value was not accepted.");
    if (reason == QLatin1String("stale-handle"))
        return QObject::tr("That item changed; the list is being refreshed.");
    if (reason == QLatin1String("unsupported"))
        return QObject::tr("This device does not support that change.");
    if (reason == QLatin1String("busy")
        || reason == QLatin1String("too-many-operations"))
        return QObject::tr("The audio service is busy; try again in a moment.");
    if (reason == QLatin1String("unavailable"))
        return QObject::tr("The audio service is not available right now.");
    if (reason == QLatin1String("owner-replaced")
        || reason == QLatin1String("authority-replaced")
        || reason == QLatin1String("pipewire-replaced")
        || reason == QLatin1String("wireplumber-replaced"))
        return QObject::tr(
            "The audio system changed while the request was in flight.");
    return QObject::tr("The %1 change was not applied.").arg(what);
}

void AudioAppletController::handleOperationCompleted(
    quint64 requestId, const OperationResult &result)
{
    const auto serialIt = m_serialByRequestId.constFind(requestId);
    if (serialIt == m_serialByRequestId.constEnd()) {
        // AGENT-GUARD: Late, foreign, or already-pruned request IDs are never
        // replayed into the UI. Reacting to an unknown completion would let a
        // stale lineage surface as fresh feedback.
        return;
    }
    const quint64 serial = *serialIt;
    const auto pendingIt = m_pendingBySerial.constFind(serial);
    const RequestKind kind = pendingIt == m_pendingBySerial.constEnd()
                               ? RequestKind::Volume
                               : pendingIt->kind;

    m_pendingBySerial.remove(serial);
    m_serialByRequestId.remove(requestId);

    const QString failure = requestFailureText(result, kind);
    if (!failure.isEmpty())
        publishFeedback(failure);
    reproject();
}

} // namespace QindaQt::Shell::AudioApplet

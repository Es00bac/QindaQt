// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

#include "audio_operation_admission_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Audio
{

using namespace Admission;
namespace
{

// A console operation needs the console slice plus the right mutator bit; the
// gain and routing bits are separate so a service can publish a read-only
// console (ADR-0173).
// What a console-capable service advertises. The gain and routing bits are
// separate so a future read-only console can publish the model without the
// mutators.
[[nodiscard]] Capabilities consoleCapabilities()
{
    return Capabilities{} | Capability::Console | Capability::SetConsoleGain
        | Capability::SetConsoleRouting | Capability::ConsoleMeters
        | Capability::ManageVbanStreams;
}



bool validBackendReasonCode(const QString &reasonCode)
{
    if (reasonCode.isEmpty()
        || !isBoundedText(reasonCode, kMaxReasonCodeUtf8Bytes)) {
        return false;
    }
    for (const QChar character : reasonCode) {
        const bool lowerAscii = character >= QLatin1Char('a')
            && character <= QLatin1Char('z');
        const bool digit = character >= QLatin1Char('0')
            && character <= QLatin1Char('9');
        if (!lowerAscii && !digit && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

const Device *findDevice(const QList<Device> &devices, const Handle &handle)
{
    for (const Device &device : devices) {
        if (device.handle == handle) {
            return &device;
        }
    }
    return nullptr;
}





} // namespace

AudioOperationCoordinator::AudioOperationCoordinator(AudioBackend *backend, QObject *parent,
                                                     QString presetDirectory, QString macroPath,
                                                     QString vbanPath)
    : QObject(parent)
    , m_backend(backend)
    , m_presets(presetDirectory.isEmpty() ? PresetStore::defaultDirectory()
                                          : std::move(presetDirectory))
    , m_macros(macroPath.isEmpty() ? MacroStore::defaultPath() : std::move(macroPath))
    , m_vban(vbanPath.isEmpty() ? VbanStore::defaultPath() : std::move(vbanPath))
{
    Q_ASSERT(m_backend != nullptr);
    m_snapshot.schemaVersion = kSchemaVersion;
    m_snapshot.epoch = 1;
    m_snapshot.revision = 1;
    m_snapshot.availability = Availability::Starting;
    m_snapshot.reasonCode = QStringLiteral("starting");

    connect(m_backend, &AudioBackend::snapshotReady, this,
            &AudioOperationCoordinator::acceptSnapshot);
    connect(m_backend, &AudioBackend::operationFinished, this,
            &AudioOperationCoordinator::acceptBackendResult);
    connect(m_backend, &AudioBackend::levelsReady, this,
            &AudioOperationCoordinator::acceptLevels);
    connect(m_backend, &AudioBackend::recordingFailed, this,
            &AudioOperationCoordinator::acceptRecordingFailure);
    connect(m_backend, &AudioBackend::vbanRunningChanged, this,
            &AudioOperationCoordinator::acceptVbanRunning);
}

const Snapshot &AudioOperationCoordinator::snapshot() const noexcept
{
    return m_snapshot;
}

void AudioOperationCoordinator::start()
{
    if (m_running) {
        return;
    }
    const quint64 generation = m_backend->start();
    if (generation == 0) {
        return;
    }
    m_backendGeneration = generation;
    m_runningVban.clear();
    m_running = true;
    // The virtual endpoints are declared from the first moment the backend
    // runs, not from the first snapshot: they are what the console is made of,
    // and a restarted backend rebuilds them from this declaration.
    m_publishedEndpoints.clear();
    publishConsoleEndpoints();
    if (m_hasBackendSnapshot) {
        publishRestartingSnapshot();
    }
}

void AudioOperationCoordinator::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_backendGeneration = 0;
    m_runningVban.clear();
    m_backend->stop();
    makePendingUncertain(m_snapshot, QStringLiteral("service-stopped"));
}

void AudioOperationCoordinator::publishRestartingSnapshot()
{
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    Snapshot restarting;
    restarting.schemaVersion = kSchemaVersion;
    restarting.epoch = m_snapshot.epoch;
    restarting.revision = m_snapshot.revision + 1;
    restarting.availability = Availability::Starting;
    restarting.reasonCode = QStringLiteral("backend-restarting");
    m_snapshot = restarting;
    m_minimumRestartEpoch = m_snapshot.epoch == std::numeric_limits<quint64>::max()
        ? m_snapshot.epoch
        : m_snapshot.epoch + 1;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

OperationResult AudioOperationCoordinator::immediate(const OperationRequest &request,
                                                     const OperationStatus status,
                                                     const QString &reasonCode) const
{
    return {
        .kind = request.kind,
        .status = status,
        .initiatingEpoch = m_snapshot.epoch,
        .initiatingRevision = m_snapshot.revision,
        .observedEpoch = m_snapshot.epoch,
        .observedRevision = m_snapshot.revision,
        .reasonCode = reasonCode,
        .diagnostic = {},
        .wireValid = true,
    };
}


OperationSubmission AudioOperationCoordinator::submit(const OperationRequest &request)
{
    // AGENT-CONTRACT: console operations are applied HERE and never submitted
    // to the graph backend (ADR-0173). The console is QindaQt's own state - the
    // backend only ever realises the routing it implies - so a console change
    // completes synchronously instead of waiting on a graph round trip, which
    // is what makes a fader feel attached to the sound.
    if (isConsoleOperation(request.kind)) {
        const QString rejection = validateRequest(request);
        if (!rejection.isEmpty()) {
            return {.pending = false,
                    .operationId = 0,
                    .immediateResult = immediate(request,
                                                 rejection == QStringLiteral("unsupported")
                                                     ? OperationStatus::Unsupported
                                                     : OperationStatus::Rejected,
                                                 rejection)};
        }
        // The two pin operations arrive with a device HANDLE and are applied
        // as a device NAME (ADR-0178): the handle is what the client holds,
        // the name is what survives a reboot. Resolved here, against the
        // retained snapshot, so the console model never learns about handles.
        OperationRequest resolved = request;
        if (request.kind == OperationKind::SetStripSource
            || request.kind == OperationKind::SetBusTarget) {
            resolved.nodeName.clear();
            if (request.primary.isValid()) {
                const Device *const device = request.kind == OperationKind::SetStripSource
                    ? findDevice(m_snapshot.inputs, request.primary)
                    : findDevice(m_snapshot.outputs, request.primary);
                // A live handle that is not a device of the right kind - an
                // output offered to a strip - is a client error, not a pin.
                if (device == nullptr || device->nodeName.isEmpty()) {
                    return {.pending = false,
                            .operationId = 0,
                            .immediateResult = immediate(request, OperationStatus::Rejected,
                                                         QStringLiteral("stale-handle"))};
                }
                resolved.nodeName = device->nodeName;
            }
        }
        QString reasonCode;
        if (request.kind == OperationKind::UpsertVbanStream) {
            const VbanStream &definition = request.vbanDefinition;
            bool busKnown = !definition.outgoing;
            if (definition.outgoing) {
                for (const Bus &bus : m_console.console().buses)
                    busKnown = busKnown || bus.id == definition.busId;
            }
            if (!busKnown || !m_vban.upsert(definition, &reasonCode)) {
                return {.pending = false, .operationId = 0,
                        .immediateResult = immediate(
                            request, OperationStatus::Rejected,
                            busKnown ? reasonCode : QStringLiteral("unknown-bus"))};
            }
            // An edit may replace the authorized source, port or speaker. It
            // always returns to disabled so saving a definition cannot quietly
            // open or retarget a live receive path.
            m_console.setVbanEnabled(definition.name, false);
        } else if (request.kind == OperationKind::DeleteVbanStream) {
            if (!m_vban.remove(request.displayName, &reasonCode)) {
                return {.pending = false, .operationId = 0,
                        .immediateResult = immediate(request, OperationStatus::Rejected,
                                                     reasonCode)};
            }
            m_console.setVbanEnabled(request.displayName, false);
        } else if (request.kind == OperationKind::SetVbanEnabled) {
            const QString name = request.displayName.trimmed();
            bool known = false;
            for (const VbanStream &stream : m_vban.load()) {
                known = known || stream.name == name;
            }
            if (!known) {
                return {.pending = false,
                        .operationId = 0,
                        .immediateResult = immediate(request, OperationStatus::Rejected,
                                                     QStringLiteral("unknown-vban-stream"))};
            }
            m_console.setVbanEnabled(name, request.enabled);
        } else if (request.kind == OperationKind::StartRecording
                   || request.kind == OperationKind::StopRecording) {
            reasonCode = applyRecordingRequest(request);
            if (!reasonCode.isEmpty()) {
                return {.pending = false,
                        .operationId = 0,
                        .immediateResult = immediate(request, OperationStatus::Rejected,
                                                     reasonCode)};
            }
        } else if (request.kind == OperationKind::RunMacro) {
            reasonCode = runMacro(request);
            if (!reasonCode.isEmpty()) {
                // AGENT-GUARD: a stopped macro keeps the actions it already
                // applied (see runMacro). The early return must not skip the
                // republish, or clients sit on a console snapshot that
                // predates the applied prefix until the next console change.
                republishConsole();
                return {.pending = false,
                        .operationId = 0,
                        .immediateResult = immediate(request, OperationStatus::Rejected,
                                                     reasonCode)};
            }
        } else if (isPresetOperation(request.kind)) {
            reasonCode = applyPreset(request);
            if (!reasonCode.isEmpty()) {
                return {.pending = false,
                        .operationId = 0,
                        .immediateResult = immediate(request, OperationStatus::Rejected,
                                                     reasonCode)};
            }
        } else if (!m_console.apply(resolved, &reasonCode)) {
            return {.pending = false,
                    .operationId = 0,
                    .immediateResult = immediate(request, OperationStatus::Rejected,
                                                 reasonCode)};
        }
        // A pin changes which device the element follows; rebind against the
        // retained graph now rather than waiting for the next publication.
        autoBindConsole(m_snapshot);
        republishConsole();
        return {.pending = false,
                .operationId = 0,
                .immediateResult = immediate(request, OperationStatus::Succeeded, {})};
    }

    const QString rejection = validateRequest(request);
    if (!rejection.isEmpty()) {
        const OperationStatus status = rejection == QStringLiteral("unsupported")
            ? OperationStatus::Unsupported
            : OperationStatus::Rejected;
        return {.pending = false,
                .operationId = 0,
                .immediateResult = immediate(request, status, rejection)};
    }
    if (m_pending.size() >= kMaxInFlightOperations || m_nextOperationId == 0
        || m_nextOperationId == std::numeric_limits<quint64>::max()) {
        return {.pending = false,
                .operationId = 0,
                .immediateResult = immediate(request, OperationStatus::Busy,
                                             QStringLiteral("too-many-operations"))};
    }

    const quint64 operationId = m_nextOperationId++;
    m_pending.insert(operationId,
                     {.kind = request.kind,
                      .epoch = m_snapshot.epoch,
                      .revision = m_snapshot.revision});
    m_backend->submit(operationId, request);
    return {.pending = true, .operationId = operationId, .immediateResult = {}};
}

void AudioOperationCoordinator::makePendingUncertain(const Snapshot &observed,
                                                      const QString &reasonCode)
{
    const auto pending = std::exchange(m_pending, {});
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        const PendingOperation &operation = it.value();
        Q_EMIT operationCompleted(
            it.key(),
            {.kind = operation.kind,
             .status = OperationStatus::Uncertain,
             .initiatingEpoch = operation.epoch,
             .initiatingRevision = operation.revision,
             .observedEpoch = observed.epoch,
             .observedRevision = observed.revision,
             .reasonCode = reasonCode,
             .diagnostic = {},
             .wireValid = true});
    }
}

void AudioOperationCoordinator::acceptSnapshot(const quint64 generation,
                                                const Snapshot &snapshot)
{
    // AGENT-GUARD: stop() and every later start supersede already queued backend
    // values. Check the run before validation so stale malformed data cannot
    // mutate even the fail-closed projection.
    if (!m_running || generation == 0 || generation != m_backendGeneration) {
        return;
    }
    // Backend epochs are numerically monotonic only within this resident
    // backend object (AudioBackend's contract). Apply the cheap lineage fence
    // before payload handling so an old callback cannot degrade current state.
    if (m_minimumRestartEpoch != 0 && snapshot.epoch < m_minimumRestartEpoch) {
        return;
    }
    if (m_hasBackendSnapshot) {
        if (snapshot.epoch < m_snapshot.epoch) {
            return;
        }
        if (snapshot.epoch == m_snapshot.epoch) {
            if (snapshot.revision < m_snapshot.revision) {
                return;
            }
            if (snapshot.revision == m_snapshot.revision) {
                // Equal lineage has one canonical value. An identical callback
                // is harmless; changed content is contradictory and is dropped.
                return;
            }
        }
    }

    const ValidationResult validation = validateSnapshot(snapshot);
    if (!validation.accepted) {
        Snapshot unavailable = m_snapshot;
        if (unavailable.revision == std::numeric_limits<quint64>::max()) {
            return;
        }
        ++unavailable.revision;
        unavailable.availability = Availability::Degraded;
        unavailable.capabilities = {};
        unavailable.reasonCode = QStringLiteral("backend-malformed");
        unavailable.diagnostic.clear();
        unavailable.defaultOutput = {};
        unavailable.defaultInput = {};
        unavailable.outputs.clear();
        unavailable.inputs.clear();
        unavailable.streams.clear();
        // The console survives a malformed backend payload: it is the user's
        // own configuration, not a projection of the graph, and blanking it
        // would lose their routing because a daemon hiccuped.
        m_runningVban.clear();
        if (!m_publishedVban.isEmpty()) {
            m_publishedVban.clear();
            m_backend->applyVban({});
        }
        unavailable.console = m_console.console();
        unavailable.console.presets = m_presets.names();
        unavailable.console.macros = MacroStore::names(m_macros.load());
        unavailable.console.recording = m_recording;
        unavailable.console.vban = vbanStreams();
        unavailable.capabilities = consoleCapabilities();
        makePendingUncertain(unavailable, QStringLiteral("backend-malformed"));
        m_snapshot = unavailable;
        m_hasBackendSnapshot = true;
        Q_EMIT snapshotChanged(m_snapshot);
        Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
        return;
    }

    if (m_hasBackendSnapshot && snapshot.epoch != m_snapshot.epoch) {
        makePendingUncertain(snapshot, QStringLiteral("authority-replaced"));
        m_runningVban.clear();
    }
    if (snapshot.availability != Availability::Ready
        && snapshot.availability != Availability::Degraded)
        m_runningVban.clear();
    m_snapshot = snapshot;
    // Follow the graph with the console's endpoints BEFORE folding the console
    // in, so this publication already carries the bindings this snapshot made
    // possible rather than showing them one revision late.
    autoBindConsole(snapshot);
    // AGENT-GUARD: the console belongs to THIS coordinator, not to the graph
    // backend, so every backend snapshot must have it folded back in. Without
    // this a device appearing or disappearing would blank the user's whole
    // console - faders, routing and all - on the next publication.
    m_snapshot.console = m_console.console();
    m_snapshot.console.presets = m_presets.names();
    m_snapshot.console.macros = MacroStore::names(m_macros.load());
    m_snapshot.console.recording = m_recording;
    m_snapshot.capabilities |= consoleCapabilities();
    // A new graph generation can make an endpoint resolvable that was not
    // before, so the routing is re-derived against every accepted snapshot
    // rather than only when the user touches the console.
    publishConsoleEndpoints();
    publishProcessing();
    publishBusProcessing();
    publishRouting();
    publishMetering();
    publishRecording();
    publishVban();
    m_snapshot.console.vban = vbanStreams();
    m_hasBackendSnapshot = true;
    m_minimumRestartEpoch = 0;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void AudioOperationCoordinator::acceptBackendResult(
    const quint64 generation, const quint64 operationId,
    const BackendOperationOutcome &outcome)
{
    if (!m_running || generation == 0 || generation != m_backendGeneration) {
        return;
    }
    const auto it = m_pending.find(operationId);
    if (it == m_pending.end()) {
        return;
    }
    const PendingOperation pending = it.value();
    m_pending.erase(it);

    OperationStatus status = OperationStatus::Failed;
    bool knownStatus = true;
    switch (outcome.status) {
    case BackendOperationStatus::Succeeded:
        status = OperationStatus::Succeeded;
        break;
    case BackendOperationStatus::Unsupported:
        status = OperationStatus::Unsupported;
        break;
    case BackendOperationStatus::Failed:
        status = OperationStatus::Failed;
        break;
    case BackendOperationStatus::Uncertain:
        status = OperationStatus::Uncertain;
        break;
    default:
        knownStatus = false;
        break;
    }
    const bool authorityReplaced = pending.epoch != m_snapshot.epoch;
    if (authorityReplaced) {
        status = OperationStatus::Uncertain;
    }

    OperationResult result{.kind = pending.kind,
                           .status = status,
                           .initiatingEpoch = pending.epoch,
                           .initiatingRevision = pending.revision,
                           .observedEpoch = m_snapshot.epoch,
                           .observedRevision = m_snapshot.revision,
                           .reasonCode = outcome.reasonCode,
                           .diagnostic = outcome.diagnostic,
                           .wireValid = true};
    if (authorityReplaced) {
        result.reasonCode = QStringLiteral("authority-replaced");
        result.diagnostic.clear();
    }
    // AGENT-GUARD: AudioBackend is an untrusted platform boundary. Never copy a
    // partially sanitized outcome to D-Bus; one invalid field replaces the
    // entire classification with a stable, protocol-valid failure.
    if (!knownStatus || !validBackendReasonCode(outcome.reasonCode)
        || !validateOperationResult(result).accepted) {
        result.status = OperationStatus::Failed;
        result.reasonCode = QStringLiteral("backend-malformed");
        result.diagnostic.clear();
    }
    Q_ASSERT(validateOperationResult(result).accepted);
    Q_EMIT operationCompleted(operationId, result);
}

} // namespace QindaQt::Audio

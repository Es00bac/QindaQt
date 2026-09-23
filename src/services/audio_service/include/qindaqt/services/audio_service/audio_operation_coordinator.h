// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/console_model.h>
#include <qindaqt/services/audio_service/macro_store.h>
#include <qindaqt/services/audio_service/preset_store.h>
#include <qindaqt/services/audio_service/vban_store.h>

#include <qindaqt/services/audio_service/audio_backend.h>

#include <QtCore/QHash>
#include <QtCore/QObject>

namespace QindaQt::Audio
{

struct OperationSubmission {
    bool pending = false;
    quint64 operationId = 0;
    OperationResult immediateResult;
};

// Owns authoritative Qt-thread publication and operation lineage. The backend
// remains policy-neutral and cannot bypass stale-handle or payload validation.
// The borrowed backend shares this object's Qt thread and must outlive it;
// stop() is a publication barrier and makes every outstanding submitted
// operation uncertain. Backend run generations and snapshot lineage prevent a
// stopped or superseded adapter run from replacing authoritative state.
class AudioOperationCoordinator : public QObject
{
    Q_OBJECT

public:
    // `presetDirectory` empty selects PresetStore::defaultDirectory(); a test
    // passes a temporary directory.
    explicit AudioOperationCoordinator(AudioBackend *backend, QObject *parent = nullptr,
                                       QString presetDirectory = {}, QString macroPath = {},
                                       QString vbanPath = {});

    [[nodiscard]] const Snapshot &snapshot() const noexcept;
    // The console this coordinator owns (ADR-0173). Exposed so a persistence
    // owner can save and restore it without going through the wire.
    [[nodiscard]] ConsoleModel &consoleModel() noexcept { return m_console; }
    [[nodiscard]] OperationSubmission submit(const OperationRequest &request);
    void start();
    void stop();

Q_SIGNALS:
    void snapshotChanged(const QindaQt::Audio::Snapshot &snapshot);
    void invalidated(quint64 epoch, quint64 revision);
    void operationCompleted(quint64 operationId,
                            const QindaQt::Audio::OperationResult &result);
    // Meter readings, forwarded at meter rate without touching lineage.
    void levelsChanged(const QList<QindaQt::Audio::LevelReading> &levels);

private Q_SLOTS:
    void acceptSnapshot(quint64 generation,
                        const QindaQt::Audio::Snapshot &snapshot);
    void acceptBackendResult(quint64 generation, quint64 operationId,
                             const QindaQt::Audio::BackendOperationOutcome &outcome);
    void acceptLevels(quint64 generation,
                      const QList<QindaQt::Audio::LevelReading> &levels);
    void acceptRecordingFailure(quint64 generation, const QString &reasonCode);
    void acceptVbanRunning(quint64 generation,
                           const QList<QindaQt::Audio::BackendVbanStream> &running);

private:
    struct PendingOperation {
        OperationKind kind = OperationKind::SetDefault;
        quint64 epoch = 0;
        quint64 revision = 0;
    };

    [[nodiscard]] OperationResult immediate(const OperationRequest &request,
                                            OperationStatus status,
                                            const QString &reasonCode) const;
    [[nodiscard]] QString validateRequest(const OperationRequest &request) const;
    // True for the operation kinds the console model owns; those never reach
    // the graph backend.
    [[nodiscard]] static bool isConsoleOperation(OperationKind kind) noexcept;
    // Presets (ADR-0182) are applied by the coordinator, not the model: they
    // are whole documents through the preset store.
    [[nodiscard]] static bool isPresetOperation(OperationKind kind) noexcept;
    [[nodiscard]] QString applyPreset(const OperationRequest &request);
    // Runs a macro's actions in order as ordinary console operations; stops
    // at the first one refused and reports its reason (ADR-0183).
    [[nodiscard]] QString runMacro(const OperationRequest &request);
    // The recorder (ADR-0184): one recording at a time, declared to the backend.
    [[nodiscard]] QString applyRecordingRequest(const OperationRequest &request);
    void publishRecording();
    // VBAN (ADR-0185): the defined streams with the user's switches, declared
    // to the backend when enabled and, for an outgoing one, bound.
    [[nodiscard]] QList<VbanStream> vbanStreams() const;
    void publishVban();
    // Republishes the snapshot with the console's current value folded in.
    void republishConsole();
    // Re-derives the backend routing from the console and the currently
    // resolvable devices, and declares it to the backend when it changed.
    // Follows the graph with the console's endpoints. Without this the console
    // is a set of faders wired to nothing: no routing is buildable and no meter
    // has a node to read, so the rack would draw but never move.
    void autoBindConsole(const Snapshot &snapshot);
    void publishRouting();
    // Re-derives which console elements are metrable right now and declares
    // them to the backend when the set changed.
    void publishMetering();
    // Declares the virtual strips' and buses' graph endpoints to the backend
    // when the set changed (ADR-0175).
    void publishConsoleEndpoints();
    // Declares the strips whose racks are active and whose device the graph
    // has (ADR-0179), when the set or any rack changed.
    void publishProcessing();
    void publishBusProcessing();
    void makePendingUncertain(const Snapshot &observed, const QString &reasonCode);
    void publishRestartingSnapshot();

    AudioBackend *m_backend = nullptr;
    Snapshot m_snapshot;
    ConsoleModel m_console;
    PresetStore m_presets;
    MacroStore m_macros;
    VbanStore m_vban;
    QList<BackendVbanStream> m_publishedVban;
    QList<BackendVbanStream> m_runningVban;
    quint64 m_nextVbanActivationToken = 1;
    Recording m_recording;
    BackendRecording m_publishedRecording;
    QList<BackendRoutingEdge> m_publishedRouting;
    QList<BackendMeterTarget> m_publishedMetering;
    QList<BackendConsoleEndpoint> m_publishedEndpoints;
    QList<BackendProcessingChain> m_publishedProcessing;
    QList<BackendBusChain> m_publishedBusProcessing;
    QHash<quint64, PendingOperation> m_pending;
    quint64 m_nextOperationId = 1;
    quint64 m_backendGeneration = 0;
    quint64 m_minimumRestartEpoch = 0;
    bool m_hasBackendSnapshot = false;
    bool m_running = false;
};

} // namespace QindaQt::Audio

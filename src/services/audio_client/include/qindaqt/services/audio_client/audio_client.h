// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_client/audio_transport.h>
#include <qindaqt/services/audio_client/retry_backoff.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <optional>
#include <utility>

namespace QindaQt::Audio
{

enum class ClientState {
    Stopped,
    Starting,
    Ready,
    Unavailable,
    Degraded,
};

// AGENT-CONTRACT: This QObject is owned and used on one Qt thread. It binds to
// an exact D-Bus owner, publishes only validated snapshots, serializes
// mutations, and never replays a timed-out or owner-interrupted mutation. The
// borrowed transport must share this object's thread and outlive it; all
// completion/error reporting is asynchronous through signals. stop() cancels
// undelivered results, then schedules one client-stopped result for a mutation
// that was still transport-backed; destruction safely drops queued delivery.
class AudioClient : public QObject
{
    Q_OBJECT

public:
    explicit AudioClient(AudioTransport *transport, QObject *parent = nullptr);

    void start();
    void stop();
    [[nodiscard]] ClientState state() const noexcept;
    [[nodiscard]] QString reasonCode() const;
    [[nodiscard]] QString owner() const;
    [[nodiscard]] bool hasSnapshot() const noexcept;
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] bool operationPending() const noexcept;

    // Meter lease (ADR-0394). The Levels stream is subscribed only while at
    // least one consumer holds a lease: retainLevels() counts one more, and
    // the first one turns the subscription on; releaseLevels() gives one back,
    // and the last one turns it off, returns every meter to "unknown" (the
    // same signal an upstream stop gives, an empty levelsChanged batch) and
    // drops a frame that was already in flight. A surface holds a lease only
    // while a meter is visible. The count survives stop()/start() and owner
    // changes: the transport re-applies the want to each owner. A release
    // with no lease held is ignored. LevelsLease is the RAII form.
    void retainLevels();
    void releaseLevels();
    [[nodiscard]] int levelsLeaseCount() const noexcept { return m_levelsLeases; }

    void setRequestTimeout(int milliseconds);
    [[nodiscard]] quint64 setDefault(const Handle &device);
    [[nodiscard]] quint64 setVolume(const Handle &target, double volume);
    [[nodiscard]] quint64 setMute(const Handle &target, bool muted);
    [[nodiscard]] quint64 moveStream(const Handle &stream, const Handle &device);
    [[nodiscard]] quint64 setChannelVolumes(const Handle &target,
                                            const QVector<double> &volumes);
    [[nodiscard]] quint64 createVirtualDevice(DeviceKind kind, const QString &displayName,
                                              quint32 channels);
    [[nodiscard]] quint64 removeVirtualDevice(const Handle &device);
    // Schema 13 (ADR-0288): remember and apply `device`'s latency offset, in
    // signed nanoseconds. Success means Audio1 remembered it; the applied
    // value is confirmed by the device's latencyOffsetNs in a later snapshot.
    [[nodiscard]] quint64 setLatencyOffset(const Handle &device, qint64 offsetNs);

    // Console operations (ADR-0173). Each addresses a strip or bus by its
    // stable console id; there is no graph handle to go stale, so a request
    // stays meaningful across the device behind it disappearing.
    [[nodiscard]] quint64 setStripGain(const QString &stripId, double gainDb);
    [[nodiscard]] quint64 setStripMuted(const QString &stripId, bool muted);
    [[nodiscard]] quint64 setStripSoloed(const QString &stripId, bool soloed);
    [[nodiscard]] quint64 setStripMono(const QString &stripId, bool mono);
    [[nodiscard]] quint64 setStripPan(const QString &stripId, double pan);
    [[nodiscard]] quint64 setStripTrim(const QString &stripId,
                                       const QVector<double> &trimDb);
    [[nodiscard]] quint64 setStripSend(const QString &stripId, quint32 busIndex,
                                       bool enabled, double gainDb);
    [[nodiscard]] quint64 setBusGain(const QString &busId, double gainDb);
    [[nodiscard]] quint64 setBusMuted(const QString &busId, bool muted);
    [[nodiscard]] quint64 setBusMono(const QString &busId, bool mono);
    [[nodiscard]] quint64 setBusTarget(const QString &busId, const Handle &device);
    // Pins a strip to a capture device (ADR-0178). An invalid handle clears
    // the pin and returns the strip to automatic binding; the same is true of
    // setBusTarget for a bus.
    [[nodiscard]] quint64 setStripSource(const QString &stripId, const Handle &device);
    // Replaces the strip's whole processing rack (ADR-0179).
    [[nodiscard]] quint64 setStripProcessing(const QString &stripId,
                                             const StripProcessing &processing);
    // Replaces the bus's rack (ADR-0180).
    [[nodiscard]] quint64 setBusProcessing(const QString &busId,
                                           const BusProcessing &processing);
    // Presets (ADR-0182); the names are in snapshot().console.presets.
    [[nodiscard]] quint64 savePreset(const QString &name);
    [[nodiscard]] quint64 loadPreset(const QString &name);
    [[nodiscard]] quint64 deletePreset(const QString &name);
    // Runs a macro button (ADR-0183); names are in snapshot().console.macros.
    [[nodiscard]] quint64 runMacro(const QString &name);
    // The recorder (ADR-0184): state is in snapshot().console.recording.
    [[nodiscard]] quint64 startRecording(const QString &busId, const QString &format);
    [[nodiscard]] quint64 stopRecording();
    // VBAN (ADR-0185): streams are in snapshot().console.vban.
    [[nodiscard]] quint64 setVbanEnabled(const QString &name, bool enabled);
    [[nodiscard]] quint64 upsertVbanStream(const VbanStream &definition);
    [[nodiscard]] quint64 deleteVbanStream(const QString &name);

    // Test seam, not for product code: the interval the next snapshot retry
    // will wait, and the timer that fires it. Tests drive the timer's
    // timeout() directly instead of sleeping through a real backoff.
    [[nodiscard]] int pendingRetryDelayMilliseconds() const { return m_retryTimer.interval(); }
    [[nodiscard]] QTimer *retryTimerForTesting() { return &m_retryTimer; }

Q_SIGNALS:
    void stateChanged(QindaQt::Audio::ClientState state, const QString &reasonCode);
    void snapshotChanged(const QindaQt::Audio::Snapshot &snapshot);
    void operationCompleted(quint64 requestId,
                            const QindaQt::Audio::OperationResult &result);
    // Meter readings, already folded into snapshot(). Emitted INSTEAD of
    // snapshotChanged: a surface that rebuilt its whole console model twenty
    // times a second would be unusable, so meters get their own narrow signal.
    void levelsChanged(const QList<QindaQt::Audio::LevelReading> &levels);

private Q_SLOTS:
    void acceptOwner(const QString &owner);
    void acceptInvalidation(const QString &owner, quint64 epoch, quint64 revision);
    void acceptSnapshotReply(const QString &owner, quint64 requestId,
                             bool transportSuccess, const Snapshot &snapshot,
                             const QString &reasonCode);
    void acceptOperationReply(const QString &owner, quint64 requestId,
                              bool transportSuccess, const OperationResult &result,
                              const QString &reasonCode);
    void acceptLevels(const QString &owner,
                      const QList<QindaQt::Audio::LevelReading> &levels);
    void onFetchTimeout();
    void onOperationTimeout();

private:
    struct PendingOperation {
        quint64 requestId = 0;
        OperationRequest request;
        quint64 epoch = 0;
        quint64 revision = 0;
    };

    void publishState(ClientState state, const QString &reasonCode);
    void publishSnapshotState(const Snapshot &snapshot);
    void requestSnapshot();
    void scheduleRefetch();
    [[nodiscard]] quint64 beginOperation(const OperationRequest &request);
    void queueOperationCompletion(quint64 requestId, OperationResult result);
    void cancelQueuedOperationCompletions();
    void completeUncertain(const QString &reasonCode);
    [[nodiscard]] OperationResult localResult(const OperationRequest &request,
                                              OperationStatus status,
                                              const QString &reasonCode) const;

    AudioTransport *m_transport = nullptr;
    ClientState m_state = ClientState::Stopped;
    QString m_reasonCode;
    QString m_owner;
    std::optional<Snapshot> m_snapshot;
    std::optional<PendingOperation> m_operation;
    QHash<quint64, OperationResult> m_queuedOperationCompletions;
    QTimer m_fetchTimer;
    QTimer m_operationTimer;
    QTimer m_retryTimer;
    // AGENT-CONTRACT (ADR-0394): a refused, lost or contradicted snapshot is
    // retried on this schedule (0.2 s doubling to 10 s), reset by any healthy
    // answer or a new owner.
    RetryBackoff m_retryBackoff;
    int m_levelsLeases = 0;
    quint64 m_nextRequestId = 1;
    quint64 m_fetchRequestId = 0;
    bool m_fetchInFlight = false;
    bool m_refetchNeeded = false;
    int m_requestTimeoutMs = 5000;
};

// RAII form of AudioClient::retainLevels: holds one lease for its lifetime.
// Movable, not copyable. The client must outlive the lease (the client is
// destroyed after the surfaces that borrow it).
class LevelsLease
{
public:
    LevelsLease() = default;
    explicit LevelsLease(AudioClient *client)
        : m_client(client)
    {
        if (m_client != nullptr) {
            m_client->retainLevels();
        }
    }
    LevelsLease(LevelsLease &&other) noexcept
        : m_client(std::exchange(other.m_client, nullptr))
    {
    }
    LevelsLease &operator=(LevelsLease &&other) noexcept
    {
        if (this != &other) {
            release();
            m_client = std::exchange(other.m_client, nullptr);
        }
        return *this;
    }
    LevelsLease(const LevelsLease &) = delete;
    LevelsLease &operator=(const LevelsLease &) = delete;
    ~LevelsLease() { release(); }

    void release()
    {
        if (m_client != nullptr) {
            std::exchange(m_client, nullptr)->releaseLevels();
        }
    }
    [[nodiscard]] bool held() const noexcept { return m_client != nullptr; }

private:
    AudioClient *m_client = nullptr;
};

// Counted leases for a surface that hands them out to many meters (ADR-0394):
// the client sees one lease per retain(), a release with nothing held is
// ignored, and the destructor gives back whatever is still held, so a surface
// torn down while meters are visible cannot leave the stream subscribed.
// Not copyable. The client must outlive the set.
class LevelsLeaseSet
{
public:
    explicit LevelsLeaseSet(AudioClient *client) noexcept
        : m_client(client)
    {
    }
    LevelsLeaseSet(const LevelsLeaseSet &) = delete;
    LevelsLeaseSet &operator=(const LevelsLeaseSet &) = delete;
    ~LevelsLeaseSet() { releaseAll(); }

    void retain()
    {
        ++m_count;
        m_client->retainLevels();
    }
    void release()
    {
        if (m_count > 0) {
            --m_count;
            m_client->releaseLevels();
        }
    }
    void releaseAll()
    {
        while (m_count > 0) {
            release();
        }
    }
    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    AudioClient *m_client;
    int m_count = 0;
};

} // namespace QindaQt::Audio

Q_DECLARE_METATYPE(QindaQt::Audio::ClientState)

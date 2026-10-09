// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include <pipewire/pipewire.h>

#include "wireplumber_graph_p.h"
#include "wireplumber_send_image_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QSet>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace QindaQt::Audio
{

namespace
{

struct TaskPayload {
    std::function<void()> task;
};

gboolean runTask(gpointer data)
{
    auto *payload = static_cast<TaskPayload *>(data);
    payload->task();
    return G_SOURCE_REMOVE;
}

void deleteTaskPayload(gpointer data)
{
    delete static_cast<TaskPayload *>(data);
}

Capabilities capabilitiesFor(WpPlugin *mixer, WpPlugin *defaultNodes,
                             WpMetadata *metadata)
{
    Capabilities result;
    if (mixer != nullptr) {
        result |= Capability::SetVolume;
        result |= Capability::SetMute;
        result |= Capability::SetChannelVolumes;
    }
    if (defaultNodes != nullptr && metadata != nullptr) {
        result |= Capability::SetDefault;
    }
    if (metadata != nullptr) {
        result |= Capability::MoveStream;
    }
    // Virtual device management needs only the connected core and object
    // manager, both of which rebuild() has established before publishing.
    result |= Capability::ManageVirtualDevices;
    // So does a node Props write; each device still says whether it has an
    // offset and a declared range to honour (ADR-0288).
    result |= Capability::SetLatencyOffset;
    return result;
}

bool payloadEqual(const Snapshot &left, Snapshot right)
{
    right.revision = left.revision;
    return left == right;
}

} // namespace

WirePlumberWorker::WirePlumberWorker(const quint64 initialEpoch,
                                     SnapshotCallback snapshotCallback,
                                     OutcomeCallback outcomeCallback,
                                     LevelsCallback levelsCallback,
                                     WirePlumberWorkerLifecycleHooks lifecycleHooks,
                                     RecordingFailedCallback recordingFailedCallback,
                                     VbanRunningCallback vbanRunningCallback)
    : m_epoch(initialEpoch == 0 ? 1 : initialEpoch)
    , m_snapshotCallback(std::move(snapshotCallback))
    , m_outcomeCallback(std::move(outcomeCallback))
    , m_levelsCallback(std::move(levelsCallback))
    , m_recordingFailedCallback(std::move(recordingFailedCallback))
    , m_vbanRunningCallback(std::move(vbanRunningCallback))
    , m_lifecycleHooks(std::move(lifecycleHooks))
{
}

WirePlumberWorker::~WirePlumberWorker()
{
    stop();
}

void WirePlumberWorker::start()
{
    std::unique_lock lock(m_lifecycleMutex);
    if (m_started) {
        return;
    }
    m_started = true;
    m_stopping = false;
    m_thread = std::thread([this] { run(); });
    m_contextReady.wait(lock, [this] { return m_context != nullptr || !m_started; });
}

void WirePlumberWorker::stop()
{
    GMainContext *context = nullptr;
    {
        std::lock_guard lock(m_lifecycleMutex);
        if (!m_started) {
            return;
        }
        m_stopping = true;
        context = m_context == nullptr ? nullptr : g_main_context_ref(m_context);
    }
    if (context != nullptr) {
        g_main_context_invoke_full(
            context, G_PRIORITY_HIGH, runTask,
            new TaskPayload{[this] {
                invalidatePending(QStringLiteral("backend-stopped"));
                cleanupCore();
                quitWhenCallbacksDrained();
            }},
            deleteTaskPayload);
        g_main_context_unref(context);
        if (m_lifecycleHooks.stopTaskQueued) {
            m_lifecycleHooks.stopTaskQueued();
        }
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    std::lock_guard lock(m_lifecycleMutex);
    m_started = false;
    m_stopping = false;
}

void WirePlumberWorker::submit(const quint64 operationId, OperationRequest request)
{
    invoke([this, operationId, request = std::move(request)] {
        submitOnWorker(operationId, request);
    });
}

void WirePlumberWorker::invoke(std::function<void()> task)
{
    GMainContext *context = nullptr;
    {
        std::lock_guard lock(m_lifecycleMutex);
        context = m_context == nullptr ? nullptr : g_main_context_ref(m_context);
    }
    if (context == nullptr) {
        return;
    }
    g_main_context_invoke_full(context, G_PRIORITY_DEFAULT, runTask,
                               new TaskPayload{std::move(task)}, deleteTaskPayload);
    g_main_context_unref(context);
}

void WirePlumberWorker::run()
{
    static std::once_flag initialized;
    std::call_once(initialized, [] { wp_init(WP_INIT_ALL); });

    GMainContext *context = g_main_context_new();
    g_main_context_push_thread_default(context);
    GMainLoop *loop = g_main_loop_new(context, FALSE);
    {
        std::lock_guard lock(m_lifecycleMutex);
        m_context = context;
        m_loop = loop;
    }
    m_contextReady.notify_all();

    m_workerRun = m_workerRun == std::numeric_limits<quint64>::max()
        ? 1
        : m_workerRun + 1;

    if (m_hasRun) {
        // AGENT-GUARD: Reusing a stopped backend creates a new service
        // authority run even when the upstream daemon never changed. Advance
        // epoch before publishing so handles from the prior run cannot revive.
        advanceEpoch();
        m_daemonSerial = 0;
        m_hadDaemon = false;
    }
    m_hasRun = true;
    setupCore();
    g_main_loop_run(loop);
    cleanupCore();

    {
        std::lock_guard lock(m_lifecycleMutex);
        m_context = nullptr;
        m_loop = nullptr;
    }
    g_main_loop_unref(loop);
    g_main_context_pop_thread_default(context);
    g_main_context_unref(context);
}

void WirePlumberWorker::onManagerInstalled(WpObjectManager *manager, gpointer data)
{
    Q_UNUSED(manager)
    auto *self = static_cast<WirePlumberWorker *>(data);
    self->m_managerInstalled = true;
    self->rebuild();
}

void WirePlumberWorker::onObjectsChanged(WpObjectManager *manager, gpointer data)
{
    Q_UNUSED(manager)
    static_cast<WirePlumberWorker *>(data)->rebuild();
}

void WirePlumberWorker::onMixerChanged(WpPlugin *plugin, guint id, gpointer data)
{
    Q_UNUSED(plugin)
    Q_UNUSED(id)
    static_cast<WirePlumberWorker *>(data)->rebuild();
}

void WirePlumberWorker::onDefaultsChanged(WpPlugin *plugin, gpointer data)
{
    Q_UNUSED(plugin)
    static_cast<WirePlumberWorker *>(data)->rebuild();
}

void WirePlumberWorker::rebuild()
{
    if (!m_managerInstalled || m_manager == nullptr) {
        return;
    }
    const quint64 daemon = WirePlumberGraph::daemonSerial(m_manager);
    if (daemon == 0) {
        if (m_daemonSerial != 0) {
            m_daemonSerial = 0;
            advanceEpoch();
            invalidatePending(QStringLiteral("wireplumber-replaced"));
        }
        publishUnavailable(QStringLiteral("wireplumber-unavailable"));
        return;
    }
    if (m_hadDaemon && m_daemonSerial != 0 && daemon != m_daemonSerial) {
        advanceEpoch();
        invalidatePending(QStringLiteral("wireplumber-replaced"));
    } else if (m_hadDaemon && m_daemonSerial == 0) {
        // Loss already advanced the epoch; reappearance uses that new lineage.
        invalidatePending(QStringLiteral("wireplumber-replaced"));
    }
    m_hadDaemon = true;
    m_daemonSerial = daemon;

    WpMetadata *metadata = WirePlumberGraph::defaultMetadata(m_manager);
    const Capabilities capabilities = capabilitiesFor(m_mixer, m_defaultNodes, metadata);
    if (metadata != nullptr) {
        g_object_unref(metadata);
    }
    // Latency first (ADR-0288): a device that just appeared gets its
    // remembered offset now, and its declared range is queried once.
    applyLatencyOnWorker();
    // AGENT-GUARD: re-apply the console's declared routing against the graph as
    // it now is. A device that just appeared makes an edge buildable that was
    // not a moment ago, and a daemon replacement destroyed every loopback this
    // worker had loaded - without this the user's matrix would come back empty
    // after a PipeWire restart even though the console still shows it.
    // Endpoints first: a virtual strip or bus that does not exist yet cannot be
    // routed or metered, and creating it publishes the snapshot that binds it.
    applyConsoleEndpointsOnWorker(m_declaredEndpoints);
    // Racks before routing and metering: both read the processed sink of a
    // strip whose rack is running.
    applyProcessingOnWorker(m_declaredProcessing);
    applyBusProcessingOnWorker(m_declaredBusProcessing);
    applyRoutingOnWorker(m_declaredRouting);
    // A recording declared before its device was in the graph starts now.
    applyRecordingOnWorker(m_declaredRecording);
    applyVbanOnWorker(m_declaredVban);
    applySendVolumes();
    // Ownership admission and quiet retirement precede strict public mapping.
    // Only this worker's pinned internal stream halves can exceed public 0..1
    // headroom or disappear from the public application-stream inventory.
    auto graph = WirePlumberGraph::buildSnapshot(m_manager, m_mixer, m_defaultNodes,
        m_epoch, m_revision + 1, capabilities, m_latencyRanges,
        ConsoleSendImage::enabled() ? &m_ownedSends.admission() : nullptr);
    if (m_ownedSends.refused() && graph.snapshot.availability == Availability::Ready) {
        graph.snapshot.availability = Availability::Degraded;
        graph.snapshot.reasonCode = QStringLiteral("console-send-refused");
    }
    if (m_apiLoadFailed && graph.snapshot.availability == Availability::Ready) {
        graph.snapshot.availability = Availability::Degraded;
        graph.snapshot.reasonCode = QStringLiteral("wireplumber-api-degraded");
    }
    // Same reasoning for meters: a device that just appeared is now readable,
    // and a daemon replacement destroyed every capture stream this worker had.
    applyMeteringOnWorker(m_declaredMetering);
    publish(std::move(graph.snapshot));
}

void WirePlumberWorker::publishUnavailable(const QString &reasonCode)
{
    Snapshot snapshot;
    snapshot.schemaVersion = kSchemaVersion;
    snapshot.epoch = m_epoch;
    snapshot.revision = m_revision + 1;
    snapshot.availability = Availability::Unavailable;
    snapshot.reasonCode = reasonCode;
    publish(std::move(snapshot));
}

void WirePlumberWorker::publish(Snapshot snapshot)
{
    if (m_lastSnapshot.has_value() && payloadEqual(*m_lastSnapshot, snapshot)) {
        return;
    }
    if (m_revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    snapshot.revision = ++m_revision;
    m_lastSnapshot = snapshot;
    m_snapshotCallback(std::move(snapshot));
}

void WirePlumberWorker::advanceEpoch()
{
    if (m_epoch != std::numeric_limits<quint64>::max()) {
        ++m_epoch;
    }
    m_lastSnapshot.reset();
}

void WirePlumberWorker::invalidatePending(const QString &reasonCode)
{
    const auto pending = std::exchange(m_pendingOperations, {});
    for (const auto &[operationId, operationEpoch] : pending) {
        Q_UNUSED(operationEpoch)
        m_outcomeCallback(operationId,
                          {.status = BackendOperationStatus::Uncertain,
                           .reasonCode = reasonCode,
                           .diagnostic = {}});
    }
}

} // namespace QindaQt::Audio

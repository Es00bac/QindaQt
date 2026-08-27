// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include "wireplumber_graph_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QSet>

#include <limits>
#include <utility>

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
    }
    if (defaultNodes != nullptr && metadata != nullptr) {
        result |= Capability::SetDefault;
    }
    if (metadata != nullptr) {
        result |= Capability::MoveStream;
    }
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
                                     OutcomeCallback outcomeCallback)
    : m_epoch(initialEpoch == 0 ? 1 : initialEpoch)
    , m_snapshotCallback(std::move(snapshotCallback))
    , m_outcomeCallback(std::move(outcomeCallback))
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
                quitWhenSyncsDrained();
            }},
            deleteTaskPayload);
        g_main_context_unref(context);
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

void WirePlumberWorker::setupCore()
{
    cleanupCore();
    m_apiLoadFailed = false;
    m_managerInstalled = false;
    m_pendingComponents = 2;
    m_core = wp_core_new(m_context, nullptr, nullptr);
    g_signal_connect(m_core, "disconnected", G_CALLBACK(onCoreDisconnected), this);

    m_manager = wp_object_manager_new();
    wp_object_manager_add_interest(m_manager, WP_TYPE_NODE, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_LINK, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_CLIENT, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_METADATA, nullptr);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_NODE,
                                               WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_LINK,
                                               WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_CLIENT,
                                               WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_METADATA,
                                               WP_OBJECT_FEATURES_ALL);
    g_signal_connect(m_manager, "installed", G_CALLBACK(onManagerInstalled), this);
    g_signal_connect(m_manager, "objects-changed", G_CALLBACK(onObjectsChanged), this);

    wp_core_load_component(m_core, "libwireplumber-module-default-nodes-api", "module",
                           nullptr, nullptr, nullptr, onComponentLoaded, this);
    wp_core_load_component(m_core, "libwireplumber-module-mixer-api", "module", nullptr,
                           nullptr, nullptr, onComponentLoaded, this);
    if (!wp_core_connect(m_core)) {
        publishUnavailable(QStringLiteral("pipewire-unavailable"));
        scheduleReconnect();
    }
}

void WirePlumberWorker::onComponentLoaded(GObject *source, GAsyncResult *result,
                                          gpointer data)
{
    auto *self = static_cast<WirePlumberWorker *>(data);
    auto *core = WP_CORE(source);
    if (core != self->m_core) {
        return;
    }
    GError *error = nullptr;
    if (!wp_core_load_component_finish(core, result, &error)) {
        self->m_apiLoadFailed = true;
        g_clear_error(&error);
    }
    if (self->m_pendingComponents > 0) {
        --self->m_pendingComponents;
    }
    if (self->m_pendingComponents == 0) {
        self->finishApiLoading();
    }
}

void WirePlumberWorker::finishApiLoading()
{
    if (m_core == nullptr || m_manager == nullptr) {
        return;
    }
    m_mixer = wp_plugin_find(m_core, "mixer-api");
    m_defaultNodes = wp_plugin_find(m_core, "default-nodes-api");
    if (m_mixer != nullptr) {
        g_object_set(m_mixer, "scale", 1, nullptr);
        g_signal_connect(m_mixer, "changed", G_CALLBACK(onMixerChanged), this);
    }
    if (m_defaultNodes != nullptr) {
        g_signal_connect(m_defaultNodes, "changed", G_CALLBACK(onDefaultsChanged), this);
    }
    wp_core_install_object_manager(m_core, m_manager);
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
    auto graph = WirePlumberGraph::buildSnapshot(m_manager, m_mixer, m_defaultNodes,
                                                 m_epoch, m_revision + 1, capabilities);
    if (m_apiLoadFailed && graph.snapshot.availability == Availability::Ready) {
        graph.snapshot.availability = Availability::Degraded;
        graph.snapshot.reasonCode = QStringLiteral("wireplumber-api-degraded");
    }
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

void WirePlumberWorker::onCoreDisconnected(WpCore *core, gpointer data)
{
    Q_UNUSED(core)
    auto *self = static_cast<WirePlumberWorker *>(data);
    if (self->m_resetScheduled) {
        return;
    }
    self->m_resetScheduled = true;
    // AGENT-GUARD: g_main_context_invoke_full() may invoke synchronously while
    // already owning the context. An idle source is required so cleanup cannot
    // destroy WpCore from inside its own "disconnected" signal emission.
    GSource *source = g_idle_source_new();
    g_source_set_callback(source, runTask,
                          new TaskPayload{[self] { self->handleDisconnected(); }},
                          deleteTaskPayload);
    g_source_attach(source, self->m_context);
    g_source_unref(source);
}

void WirePlumberWorker::handleDisconnected()
{
    m_resetScheduled = false;
    advanceEpoch();
    m_daemonSerial = 0;
    invalidatePending(QStringLiteral("pipewire-replaced"));
    publishUnavailable(QStringLiteral("pipewire-unavailable"));
    cleanupCore();
    scheduleReconnect();
}

void WirePlumberWorker::scheduleReconnect()
{
    if (m_context == nullptr || m_stopping) {
        return;
    }
    GSource *source = g_timeout_source_new(250);
    g_source_set_callback(
        source, runTask,
        new TaskPayload{[this] {
            if (!m_stopping) {
                setupCore();
            }
        }},
        deleteTaskPayload);
    g_source_attach(source, m_context);
    g_source_unref(source);
}

void WirePlumberWorker::cleanupCore()
{
    cancelOperationSyncs();
    m_managerInstalled = false;
    if (m_mixer != nullptr) {
        g_signal_handlers_disconnect_by_data(m_mixer, this);
        g_clear_object(&m_mixer);
    }
    if (m_defaultNodes != nullptr) {
        g_signal_handlers_disconnect_by_data(m_defaultNodes, this);
        g_clear_object(&m_defaultNodes);
    }
    if (m_manager != nullptr) {
        g_signal_handlers_disconnect_by_data(m_manager, this);
        g_clear_object(&m_manager);
    }
    if (m_core != nullptr) {
        g_signal_handlers_disconnect_by_data(m_core, this);
        wp_core_disconnect(m_core);
        g_clear_object(&m_core);
    }
}

} // namespace QindaQt::Audio

// SPDX-License-Identifier: GPL-3.0-or-later
// Core/API/disconnect ownership stays separate from thread lifecycle and
// snapshot projection. Cancellation, callback draining and deferred teardown
// preserve the existing stop barrier. Send admission is confined to the
// default-OFF dependency-qualified route collaborator (ADR0361).
#include "wireplumber_worker_p.h"
#include "wireplumber_graph_p.h"
#include <pipewire/pipewire.h>
#include <memory>
#include <utility>

namespace QindaQt::Audio
{
struct WirePlumberWorker::ComponentLoad {
    WirePlumberWorker *worker = nullptr;
    WpCore *core = nullptr;
    GCancellable *cancellable = nullptr;
    bool cancelled = false;

    ~ComponentLoad()
    {
        g_clear_object(&cancellable);
    }
};

struct WirePlumberWorker::DisconnectReset {
    WirePlumberWorker *worker = nullptr;
    quint64 workerRun = 0;
    GSource *source = nullptr;
};

void WirePlumberWorker::setupCore()
{
    cleanupCore();
    m_apiLoadFailed = false;
    m_managerInstalled = false;
    m_pendingComponents = 0;
    m_core = wp_core_new(m_context, nullptr, nullptr);
    g_signal_connect(m_core, "disconnected", G_CALLBACK(onCoreDisconnected), this);

    m_manager = wp_object_manager_new();
    wp_object_manager_add_interest(m_manager, WP_TYPE_NODE, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_LINK, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_CLIENT, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_METADATA, nullptr);
    // PARAM_PROPS keeps each node's Props cached, so a device's latency
    // offset (ADR-0288) is read without a round trip and changes are signalled.
    wp_object_manager_request_object_features(
        m_manager, WP_TYPE_NODE,
        WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL | WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_LINK,
                                               WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_CLIENT,
                                               WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_METADATA,
                                               WP_OBJECT_FEATURES_ALL);
    g_signal_connect(m_manager, "installed", G_CALLBACK(onManagerInstalled), this);
    g_signal_connect(m_manager, "objects-changed", G_CALLBACK(onObjectsChanged), this);
    g_signal_connect(m_manager, "object-added", G_CALLBACK(onObjectAdded), this);
    g_signal_connect(m_manager, "object-removed", G_CALLBACK(onObjectRemoved), this);

    if (!wp_core_connect(m_core)) {
        publishUnavailable(QStringLiteral("pipewire-unavailable"));
        scheduleReconnect();
        return;
    }

    // AGENT-GUARD: Start asynchronous API loads only after PipeWire accepted
    // this core. Starting them against an unreachable runtime makes every
    // rapid stop wait for loader cancellation and used to leak one GWakeup
    // pipe pair per attempt. Connected loads remain explicitly cancellable and
    // are drained before the worker thread can exit.
    m_pendingComponents = 2;
    beginComponentLoad("libwireplumber-module-default-nodes-api");
    beginComponentLoad("libwireplumber-module-mixer-api");
}

void WirePlumberWorker::beginComponentLoad(const char *component)
{
    auto *state = new ComponentLoad{.worker = this,
                                    .core = m_core,
                                    .cancellable = g_cancellable_new(),
                                    .cancelled = false};
    m_componentLoads.insert(state);
    wp_core_load_component(m_core, component, "module", nullptr, nullptr,
                           state->cancellable, onComponentLoaded, state);
}

void WirePlumberWorker::onComponentLoaded(GObject *source, GAsyncResult *result,
                                          gpointer data)
{
    std::unique_ptr<ComponentLoad> state(static_cast<ComponentLoad *>(data));
    WirePlumberWorker *self = state->worker;
    self->m_componentLoads.erase(state.get());
    auto *core = WP_CORE(source);
    GError *error = nullptr;
    const bool succeeded = wp_core_load_component_finish(core, result, &error) != FALSE;
    g_clear_error(&error);
    if (!state->cancelled && core == state->core && core == self->m_core
        && !succeeded) {
        self->m_apiLoadFailed = true;
    }
    if (!state->cancelled && core == state->core && core == self->m_core
        && self->m_pendingComponents > 0) {
        --self->m_pendingComponents;
    }
    if (!state->cancelled && core == state->core && core == self->m_core
        && self->m_pendingComponents == 0) {
        self->finishApiLoading();
    }
    self->quitWhenCallbacksDrained();
}

void WirePlumberWorker::cancelComponentLoads()
{
    // AGENT-GUARD: The component loader owns callback data and a WpCore
    // reference until finish runs. Cancel but retain every state, then keep the
    // GLib loop alive until callbacks consume them; otherwise rapid stop leaks
    // the loader's wakeup pipes and leaves raw worker pointers outstanding.
    for (ComponentLoad *state : m_componentLoads) {
        state->cancelled = true;
        g_cancellable_cancel(state->cancellable);
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

void WirePlumberWorker::onCoreDisconnected(WpCore *core, gpointer data)
{
    Q_UNUSED(core)
    auto *self = static_cast<WirePlumberWorker *>(data);
    if (self->m_disconnectResetSource != nullptr || self->m_stopping) {
        return;
    }
    // AGENT-GUARD: g_main_context_invoke_full() may invoke synchronously while
    // already owning the context. An idle source is required so cleanup cannot
    // destroy WpCore from inside its own "disconnected" signal emission. The
    // worker retains and explicitly cancels this source: a boolean latch alone
    // can survive context teardown when stop supersedes the idle callback.
    GSource *source = g_idle_source_new();
    auto *reset = new DisconnectReset{.worker = self,
                                      .workerRun = self->m_workerRun,
                                      .source = source};
    self->m_disconnectResetSource = source;
    g_source_set_callback(source, dispatchDisconnectReset, reset,
                          deleteDisconnectReset);
    if (g_source_attach(source, self->m_context) == 0) {
        self->cancelDisconnectReset();
        return;
    }
    if (self->m_lifecycleHooks.disconnectResetScheduled) {
        self->m_lifecycleHooks.disconnectResetScheduled();
    }
}

gboolean WirePlumberWorker::dispatchDisconnectReset(gpointer data)
{
    auto *reset = static_cast<DisconnectReset *>(data);
    WirePlumberWorker *self = reset->worker;
    if (self->m_disconnectResetSource == reset->source) {
        self->m_disconnectResetSource = nullptr;
        // The context retains the dispatching source until this callback
        // returns; release the worker's creator reference here.
        g_source_unref(reset->source);
    }
    self->handleDisconnected(reset->workerRun);
    return G_SOURCE_REMOVE;
}

void WirePlumberWorker::deleteDisconnectReset(gpointer data)
{
    delete static_cast<DisconnectReset *>(data);
}

void WirePlumberWorker::cancelDisconnectReset()
{
    GSource *source = std::exchange(m_disconnectResetSource, nullptr);
    if (source == nullptr) {
        return;
    }
    g_source_destroy(source);
    g_source_unref(source);
}

void WirePlumberWorker::handleDisconnected(const quint64 workerRun)
{
    // AGENT-GUARD: Deferred source data belongs to exactly one worker run. Even
    // if later source-management code regresses, stale work must never advance
    // epoch, invalidate operations, or tear down a restarted core.
    if (workerRun != m_workerRun || m_stopping) {
        return;
    }
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
        source, [](gpointer data) -> gboolean {
            auto *self = static_cast<WirePlumberWorker *>(data);
            if (!self->m_stopping) self->setupCore();
            return G_SOURCE_REMOVE;
        }, this, nullptr);
    g_source_attach(source, m_context);
    g_source_unref(source);
}

void WirePlumberWorker::cleanupCore()
{
    // Anything still queued for deferred destruction goes with the core, now
    // and synchronously: after this the context it lives in is gone.
    if (m_moduleDestroySource != nullptr) {
        g_source_destroy(m_moduleDestroySource);
        g_source_unref(std::exchange(m_moduleDestroySource, nullptr));
    }
    flushPendingModuleDestroys();
    // AGENT-GUARD: unload the console's loopbacks before the core goes. They
    // are loaded into THIS pw_context; leaving them would either leak modules
    // across a reconnect or destroy them against a context that no longer
    // exists. The declared routing is kept so the next connection rebuilds it.
    // A recording ends with the core: its stream lives in this context. The
    // file it wrote stays complete.
    m_recorder.stop();
    stopAllVban();
    unloadAllRouting();
    unloadAllProcessing();
    unloadAllEndpoints();
    // The meter streams belong to this pw_context too, and outlive it no
    // better than the loopbacks do.
    stopMeterPolling();
    m_meters.clear();
    m_lastLevels.clear();
    cancelDisconnectReset();
    cancelComponentLoads();
    cancelNodeActivations();
    cancelOperationSyncs();
    stopLatencyOnWorker();
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

// SPDX-License-Identifier: GPL-3.0-or-later

// Per-device latency offsets on the graph worker (ADR-0288): applies the
// coordinator's declared node.name -> offset map to device nodes as they
// appear, answers a device that resets the offset with a bounded re-write
// (LatencyPolicy::shouldWrite), and queries each device's declared range once.
#include "wireplumber_worker_p.h"

#include "wireplumber_latency_p.h"

#include <pipewire/keys.h>

#include <memory>

namespace QindaQt::Audio
{

struct WirePlumberWorker::LatencyRangeQuery {
    WirePlumberWorker *worker = nullptr;
    quint64 serial = 0;
    // Identity only, never dereferenced: a completion for a replaced core is
    // dropped.
    WpCore *core = nullptr;
    GCancellable *cancellable = nullptr;
    bool cancelled = false;

    ~LatencyRangeQuery() { g_clear_object(&cancellable); }
};

namespace
{

[[nodiscard]] bool isDeviceNode(WpPipewireObject *node)
{
    const gchar *mediaClass = wp_pipewire_object_get_property(node, PW_KEY_MEDIA_CLASS);
    return mediaClass != nullptr
        && (g_str_has_prefix(mediaClass, "Audio/Sink")
            || g_str_has_prefix(mediaClass, "Audio/Source"));
}

[[nodiscard]] std::optional<quint64> serialOf(WpPipewireObject *node)
{
    const gchar *raw = wp_pipewire_object_get_property(node, PW_KEY_OBJECT_SERIAL);
    gchar *end = nullptr;
    const guint64 value = raw == nullptr ? 0 : g_ascii_strtoull(raw, &end, 10);
    if (value == 0 || end == raw || end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return static_cast<quint64>(value);
}

} // namespace

void WirePlumberWorker::applyLatencyOffsets(QList<BackendLatencyOffset> offsets)
{
    invoke([this, offsets = std::move(offsets)] {
        m_declaredLatency.clear();
        for (const BackendLatencyOffset &offset : offsets) {
            m_declaredLatency.insert_or_assign(offset.nodeName.toStdString(), offset.offsetNs);
        }
        applyLatencyOnWorker();
    });
}

void WirePlumberWorker::applyLatencyOnWorker()
{
    if (m_manager == nullptr || !m_managerInstalled) {
        return;
    }
    std::unordered_set<quint64> present;
    WpIterator *iterator =
        wp_object_manager_new_filtered_iterator(m_manager, WP_TYPE_NODE, nullptr);
    GValue value = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &value)) {
        auto *node = WP_PIPEWIRE_OBJECT(g_value_get_object(&value));
        const std::optional<quint64> serial = serialOf(node);
        if (serial.has_value() && isDeviceNode(node)) {
            present.insert(*serial);
            const std::optional<qint64> observed = WirePlumberLatency::readOffset(node);
            if (observed.has_value() && !m_latencyRanges.contains(*serial)
                && !m_latencyRangePending.contains(*serial)) {
                queryLatencyRange(node, *serial);
            }
            const gchar *name = wp_pipewire_object_get_property(node, PW_KEY_NODE_NAME);
            const auto declared =
                name == nullptr ? m_declaredLatency.end() : m_declaredLatency.find(name);
            const auto range = m_latencyRanges.find(*serial);
            // AGENT-GUARD: never write a value the node's own declaration
            // rejects. ALSA accepts a negative write although it declares
            // 0..2 s, and would then report a nonsensical latency.
            const bool admitted = declared != m_declaredLatency.end()
                && range != m_latencyRanges.end() && range->second.has_value()
                && declared->second >= range->second->minNs
                && declared->second <= range->second->maxNs;
            if (admitted
                && LatencyPolicy::shouldWrite(m_latencyReconcile[*serial], declared->second,
                                              observed)) {
                (void)WirePlumberLatency::writeOffset(node, declared->second);
            }
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);
    // A departed node takes its memory with it: a returning device is a new
    // serial with a fresh write budget and a fresh range query.
    std::erase_if(m_latencyReconcile,
                  [&present](const auto &entry) { return !present.contains(entry.first); });
    std::erase_if(m_latencyRanges,
                  [&present](const auto &entry) { return !present.contains(entry.first); });
}

void WirePlumberWorker::queryLatencyRange(WpPipewireObject *node, const quint64 serial)
{
    auto *state = new LatencyRangeQuery{.worker = this,
                                        .serial = serial,
                                        .core = m_core,
                                        .cancellable = g_cancellable_new(),
                                        .cancelled = false};
    m_latencyRangeQueries.insert(state);
    m_latencyRangePending.insert(serial);
    // WirePlumber caches no PropInfo, so the range is one asynchronous
    // enumeration per node lifetime. The task holds the node until it ends.
    wp_pipewire_object_enum_params(node, "PropInfo", nullptr, state->cancellable,
                                   onLatencyRangeQueried, state);
}

void WirePlumberWorker::onLatencyRangeQueried(GObject *source, GAsyncResult *result,
                                              gpointer data)
{
    std::unique_ptr<LatencyRangeQuery> state(static_cast<LatencyRangeQuery *>(data));
    WirePlumberWorker *worker = state->worker;
    worker->m_latencyRangeQueries.erase(state.get());
    GError *error = nullptr;
    WpIterator *propInfo =
        wp_pipewire_object_enum_params_finish(WP_PIPEWIRE_OBJECT(source), result, &error);
    g_clear_error(&error);
    if (!state->cancelled && state->core == worker->m_core) {
        worker->m_latencyRangePending.erase(state->serial);
        worker->m_latencyRanges.insert_or_assign(state->serial,
                                                 WirePlumberLatency::parseRange(propInfo));
        worker->rebuild();
    }
    if (propInfo != nullptr) {
        wp_iterator_unref(propInfo);
    }
    worker->quitWhenCallbacksDrained();
}

void WirePlumberWorker::onObjectAdded(WpObjectManager *manager, gpointer object, gpointer data)
{
    Q_UNUSED(manager)
    if (WP_IS_NODE(object) && isDeviceNode(WP_PIPEWIRE_OBJECT(object))) {
        g_signal_connect(object, "params-changed", G_CALLBACK(onNodeParamsChanged), data);
    }
}

void WirePlumberWorker::onObjectRemoved(WpObjectManager *manager, gpointer object,
                                        gpointer data)
{
    Q_UNUSED(manager)
    g_signal_handlers_disconnect_by_data(object, data);
}

void WirePlumberWorker::onNodeParamsChanged(WpPipewireObject *node, const gchar *id,
                                            gpointer data)
{
    auto *self = static_cast<WirePlumberWorker *>(data);
    if (g_strcmp0(id, "Props") != 0 || !self->m_lastSnapshot.has_value()) {
        return;
    }
    // Volume and mute already arrive through mixer-api. Only a changed offset
    // needs this rebuild, which also lets the reconcile policy answer a device
    // that reset the offset underneath Audio1.
    const std::optional<quint64> serial = serialOf(node);
    const std::optional<qint64> observed = WirePlumberLatency::readOffset(node);
    for (const QList<Device> *devices :
         {&self->m_lastSnapshot->outputs, &self->m_lastSnapshot->inputs}) {
        for (const Device &device : *devices) {
            if (serial != device.handle.serial) {
                continue;
            }
            if (observed.has_value() != device.latencyOffsetKnown
                || (observed.has_value() && *observed != device.latencyOffsetNs)) {
                self->rebuild();
            }
            return;
        }
    }
    self->rebuild();
}

void WirePlumberWorker::stopLatencyOnWorker()
{
    if (m_manager != nullptr) {
        WpIterator *iterator =
            wp_object_manager_new_filtered_iterator(m_manager, WP_TYPE_NODE, nullptr);
        GValue value = G_VALUE_INIT;
        while (wp_iterator_next(iterator, &value)) {
            g_signal_handlers_disconnect_by_data(g_value_get_object(&value), this);
            g_value_unset(&value);
        }
        wp_iterator_unref(iterator);
    }
    // AGENT-GUARD: like component loads and node activations, a PropInfo
    // enumeration owns its callback data until finish runs. Cancel but retain
    // every state; quitWhenCallbacksDrained keeps the loop alive until each
    // callback has released its own.
    for (LatencyRangeQuery *state : m_latencyRangeQueries) {
        state->cancelled = true;
        g_cancellable_cancel(state->cancellable);
    }
    m_latencyRangePending.clear();
    m_latencyRanges.clear();
    m_latencyReconcile.clear();
}

} // namespace QindaQt::Audio

// SPDX-License-Identifier: GPL-3.0-or-later
// Manual owning diagnostic, never a CTest. No backend/worker is constructed:
// those collaborators realise saved console state and are not read-only.
#include "wireplumber_graph_p.h"
#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTextStream>
#include <cmath>
#include <cstdio>

using namespace QindaQt::Audio;
namespace {
struct Observer {
    GMainLoop *loop = nullptr;
    WpCore *core = nullptr;
    WpObjectManager *manager = nullptr;
    WpPlugin *mixer = nullptr;
    WpPlugin *defaults = nullptr;
    GCancellable *cancel = nullptr;
    int pending = 2;
    bool failed = false;
    bool stopping = false;
    bool installed = false;
    guint settle = 0;
    guint timeout = 0;
    int exitCode = 3;
};

void output(const QJsonObject &value)
{
    const QByteArray bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void stop(Observer &state)
{
    state.stopping = true;
    g_cancellable_cancel(state.cancel);
    if (state.pending == 0) g_main_loop_quit(state.loop);
}

template<class Row>
QJsonObject shape(const Row &row)
{
    bool finite = std::isfinite(row.volume);
    for (double level : row.channelVolumes) finite = finite && std::isfinite(level);
    return {{"mapCount", static_cast<int>(row.channelMap.size())},
            {"volumeCount", static_cast<int>(row.channelVolumes.size())},
            {"aggregateKnown", row.volumeKnown}, {"muteKnown", row.muteKnown},
            {"canSetVolume", row.canSetVolume}, {"canSetMute", row.canSetMute},
            {"finite", finite}, {"wireValid", row.wireValid}};
}

gboolean observe(gpointer data)
{
    auto &state = *static_cast<Observer *>(data);
    state.settle = 0;
    Capabilities caps = Capability::SetVolume | Capability::SetMute
        | Capability::SetChannelVolumes | Capability::SetDefault
        | Capability::MoveStream | Capability::ManageVirtualDevices
        | Capability::SetLatencyOffset;
    const auto built = WirePlumberGraph::buildSnapshot(
        state.manager, state.mixer, state.defaults, 1, 1, caps, {});
    const Snapshot &snapshot = built.snapshot;
    const auto validation = validateSnapshot(snapshot);
    QJsonArray rows;
    auto baseline = [&] {
        Snapshot one = snapshot;
        one.outputs.clear(); one.inputs.clear(); one.streams.clear();
        one.defaultOutput = {}; one.defaultInput = {};
        return one;
    };
    auto devices = [&](const QList<Device> &values, const bool input) {
        for (qsizetype index = 0; index < values.size(); ++index) {
            Device row = values.at(index);
            row.isDefault = false;
            Snapshot one = baseline();
            (input ? one.inputs : one.outputs).append(row);
            QJsonObject item = shape(row);
            item.insert("kind", input ? "input" : "output");
            item.insert("index", static_cast<int>(index));
            item.insert("nameBytes", row.name.toUtf8().size());
            item.insert("descriptionBytes", row.description.toUtf8().size());
            item.insert("nodeNameBytes", row.nodeName.toUtf8().size());
            item.insert("latencyValid", validDeviceLatency(row, caps));
            item.insert("validation", validateSnapshot(one).reasonCode);
            rows.append(item);
        }
    };
    devices(snapshot.outputs, false); devices(snapshot.inputs, true);
    for (qsizetype index = 0; index < snapshot.streams.size(); ++index) {
        Stream row = snapshot.streams.at(index);
        row.targetKnown = false; row.target = {};
        Snapshot one = baseline(); one.streams.append(row);
        QJsonObject item = shape(row);
        item.insert("kind", "stream"); item.insert("index", static_cast<int>(index));
        item.insert("applicationBytes", row.applicationName.toUtf8().size());
        item.insert("mediaBytes", row.mediaName.toUtf8().size());
        item.insert("validation", validateSnapshot(one).reasonCode);
        rows.append(item);
    }
    output({{"observer", "audio-graph-readonly-v1"}, {"schema", static_cast<int>(snapshot.schemaVersion)},
            {"accepted", validation.accepted}, {"validation", validation.reasonCode},
            {"boundedOrMalformed", built.truncatedOrMalformed},
            {"outputs", static_cast<int>(snapshot.outputs.size())},
            {"inputs", static_cast<int>(snapshot.inputs.size())},
            {"streams", static_cast<int>(snapshot.streams.size())},
            {"rows", rows}});
    state.exitCode = validation.accepted ? 0 : 2;
    stop(state);
    return G_SOURCE_REMOVE;
}

void installed(WpObjectManager *, gpointer data)
{
    auto &state = *static_cast<Observer *>(data);
    state.installed = true;
    // A bounded observation window, not a claim of a stable or complete graph.
    if (!state.stopping) state.settle = g_timeout_add(1500, observe, &state);
}

void loaded(GObject *source, GAsyncResult *result, gpointer data)
{
    auto &state = *static_cast<Observer *>(data);
    GError *error = nullptr;
    if (!wp_core_load_component_finish(WP_CORE(source), result, &error)) state.failed = true;
    g_clear_error(&error);
    --state.pending;
    if (state.pending != 0) return;
    if (state.stopping) { g_main_loop_quit(state.loop); return; }
    state.mixer = wp_plugin_find(state.core, "mixer-api");
    state.defaults = wp_plugin_find(state.core, "default-nodes-api");
    if (state.failed || !state.mixer || !state.defaults) {
        output({{"error", "api-load-failed"}}); stop(state); return;
    }
    // Client-local interpretation only. Never emit set-volume/default actions.
    g_object_set(state.mixer, "scale", 1, nullptr);
    wp_core_install_object_manager(state.core, state.manager);
}

gboolean timedOut(gpointer data)
{
    auto &state = *static_cast<Observer *>(data);
    state.timeout = 0;
    output({{"error", "observation-timeout"}});
    stop(state);
    return G_SOURCE_REMOVE;
}

void disconnected(WpCore *, gpointer data)
{
    auto &state = *static_cast<Observer *>(data);
    if (!state.stopping) {
        output({{"error", "core-disconnected"}});
        stop(state);
    }
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments() != QStringList{application.arguments().first(), "--observe"}) {
        output({{"usage", "audio_graph_observer --observe"}, {"readOnly", true}});
        return 64;
    }
    wp_init(WP_INIT_ALL);
    Observer state;
    state.loop = g_main_loop_new(nullptr, FALSE);
    state.cancel = g_cancellable_new();
    state.core = wp_core_new(nullptr, nullptr, nullptr);
    state.manager = wp_object_manager_new();
    wp_object_manager_add_interest(state.manager, WP_TYPE_NODE, nullptr);
    wp_object_manager_add_interest(state.manager, WP_TYPE_LINK, nullptr);
    wp_object_manager_add_interest(state.manager, WP_TYPE_CLIENT, nullptr);
    wp_object_manager_add_interest(state.manager, WP_TYPE_METADATA, nullptr);
    wp_object_manager_request_object_features(state.manager, WP_TYPE_NODE,
        WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL | WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS);
    wp_object_manager_request_object_features(state.manager, WP_TYPE_LINK, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(state.manager, WP_TYPE_CLIENT, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(state.manager, WP_TYPE_METADATA, WP_OBJECT_FEATURES_ALL);
    g_signal_connect(state.manager, "installed", G_CALLBACK(installed), &state);
    g_signal_connect(state.core, "disconnected", G_CALLBACK(disconnected), &state);
    if (!wp_core_connect(state.core)) {
        state.pending = 0; output({{"error", "core-unavailable"}});
    } else {
        wp_core_load_component(state.core, "libwireplumber-module-default-nodes-api",
            "module", nullptr, nullptr, state.cancel, loaded, &state);
        wp_core_load_component(state.core, "libwireplumber-module-mixer-api",
            "module", nullptr, nullptr, state.cancel, loaded, &state);
        state.timeout = g_timeout_add(10000, timedOut, &state);
        g_main_loop_run(state.loop);
    }
    // Cancelled loaders are drained before this stack-owned callback data dies.
    if (state.settle != 0) g_source_remove(state.settle);
    if (state.timeout != 0) g_source_remove(state.timeout);
    g_clear_object(&state.mixer); g_clear_object(&state.defaults);
    wp_core_disconnect(state.core);
    g_clear_object(&state.manager); g_clear_object(&state.core);
    g_clear_object(&state.cancel); g_main_loop_unref(state.loop);
    return state.exitCode;
}

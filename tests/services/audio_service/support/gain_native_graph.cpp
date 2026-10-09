// SPDX-License-Identifier: GPL-3.0-or-later
#include "gain_native_graph.h"
#include "gain_private_runtime.h"
#include "wireplumber_send_nodes_p.h"

#include <pipewire/impl-module.h>
#include <spa/param/props.h>
#include <spa/pod/iter.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <cmath>
#include <utility>
#include <vector>

namespace QindaQt::Audio::Test
{
struct GainNativeGraph::LoadState { bool done = false; bool succeeded = false; };
struct GainNativeGraph::ModuleHold {
    pw_impl_module *module = nullptr;
    spa_hook listener{};
    ~ModuleHold() { if (module != nullptr) pw_impl_module_destroy(module); }
};
struct GainNativeGraph::NodeHold {
    pw_proxy *proxy = nullptr;
    spa_hook listener{};
    ~NodeHold() { if (proxy != nullptr) pw_proxy_destroy(proxy); }
};
GainNativeGraph::GainNativeGraph(const PrivateGainRuntime &runtime) : m_runtime(runtime) {}
GainNativeGraph::~GainNativeGraph()
{
    m_modules.clear();
    m_nodes.clear();
    if (m_cancel) g_cancellable_cancel(m_cancel);
    // A timed-out loader keeps no pointer to this fixture: its finish callback
    // owns a shared completion state plus GAsyncResult's Core reference.
    if (m_context) (void)wait([this] {
        for (const auto &state : m_loads) if (!state->done) return false;
        return true;
    }, 2000);
    if (m_manager) g_signal_handlers_disconnect_by_data(m_manager, this);
    g_clear_object(&m_mixer); g_clear_object(&m_defaults);
    if (m_core) wp_core_disconnect(m_core);
    g_clear_object(&m_manager); g_clear_object(&m_core); g_clear_object(&m_cancel);
    if (m_pushed) g_main_context_pop_thread_default(m_context);
    if (m_context) g_main_context_unref(m_context);
}
void GainNativeGraph::loaded(GObject *source, GAsyncResult *result, gpointer data)
{
    std::unique_ptr<std::shared_ptr<LoadState>> hold(
        static_cast<std::shared_ptr<LoadState> *>(data));
    GError *error = nullptr;
    (*hold)->succeeded = wp_core_load_component_finish(WP_CORE(source), result, &error) != FALSE;
    g_clear_error(&error);
    (*hold)->done = true;
}
void GainNativeGraph::installed(WpObjectManager *, gpointer data)
{
    static_cast<GainNativeGraph *>(data)->m_installed = true;
}
bool GainNativeGraph::start()
{
    if (!m_runtime.ready() || m_core != nullptr) {
        m_failure = QStringLiteral("private-runtime-or-repeated-start"); return false;
    }
    if (!m_runtime.image().bootstrapStillPinned()) {
        m_failure = QStringLiteral("qualified-bootstrap-lost"); return false;
    }
    wp_init(WP_INIT_ALL);
    m_context = g_main_context_new();
    g_main_context_push_thread_default(m_context);
    m_pushed = true;
    m_core = wp_core_new(m_context, nullptr, nullptr);
    m_cancel = g_cancellable_new();
    m_manager = wp_object_manager_new();
    wp_object_manager_add_interest(m_manager, WP_TYPE_NODE, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_LINK, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_CLIENT, nullptr);
    wp_object_manager_add_interest(m_manager, WP_TYPE_METADATA, nullptr);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_NODE,
        WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL | WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_LINK, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_CLIENT, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    wp_object_manager_request_object_features(m_manager, WP_TYPE_METADATA, WP_OBJECT_FEATURES_ALL);
    g_signal_connect(m_manager, "installed", G_CALLBACK(installed), this);
    if (!wp_core_connect(m_core)) { m_failure = QStringLiteral("private-connect"); return false; }
    for (const char *module : {"libwireplumber-module-default-nodes-api",
                               "libwireplumber-module-mixer-api"}) {
        auto state = std::make_shared<LoadState>();
        m_loads.append(state);
        wp_core_load_component(m_core, module, "module", nullptr, nullptr, m_cancel,
                               loaded, new std::shared_ptr<LoadState>(state));
    }
    if (!wait([this] { for (const auto &state : m_loads) if (!state->done) return false;
                      return true; })) {
        m_failure = QStringLiteral("private-api-deadline"); return false;
    }
    for (const auto &state : m_loads) if (!state->succeeded) {
        m_failure = QStringLiteral("private-api-load"); return false;
    }
    m_mixer = wp_plugin_find(m_core, "mixer-api");
    m_defaults = wp_plugin_find(m_core, "default-nodes-api");
    if (!m_mixer || !m_defaults) { m_failure = QStringLiteral("private-api-missing"); return false; }
    g_object_set(m_mixer, "scale", 1, nullptr);
    wp_core_install_object_manager(m_core, m_manager);
    if (!wait([this] { return m_installed && producer().valid(); })) {
        m_failure = QStringLiteral("private-producer-deadline"); return false;
    }
    return true;
}
bool GainNativeGraph::wait(const std::function<bool()> &condition, const int timeoutMs)
{
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        for (int iteration = 0; iteration < 64
             && g_main_context_iteration(m_context, FALSE); ++iteration) {}
        QThread::msleep(1);
    }
    return condition();
}
void GainNativeGraph::settle(const int milliseconds)
{
    QElapsedTimer timer; timer.start();
    (void)wait([&] { return timer.elapsed() >= milliseconds; }, milliseconds + 100);
}
SendProducerIdentity GainNativeGraph::producer(const quint64 generation) const
{
    return sendProducerIdentity(m_core, m_manager, generation);
}
QList<ObservedSendNode> GainNativeGraph::nodes() const { return observedSendNodes(m_manager); }
bool GainNativeGraph::createSink(const QString &name)
{
    if (!wp_core_is_connected(m_core)) return false;
    const QByteArray encoded = name.toUtf8();
    pw_properties *properties = pw_properties_new("factory.name", "support.null-audio-sink",
        "node.name", encoded.constData(), "node.description", "Private console gain fixture",
        "media.class", "Audio/Sink", "audio.position", "[ FL FR ]",
        "monitor.channel-volumes", "true", "node.autoconnect", "false", nullptr);
    auto *proxy = static_cast<pw_proxy *>(pw_core_create_object(wp_core_get_pw_core(m_core),
        "adapter", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, &properties->dict, 0));
    pw_properties_free(properties);
    if (!proxy) return false;
    auto hold = std::make_unique<NodeHold>();
    hold->proxy = proxy;
    static const pw_proxy_events events = [] {
        pw_proxy_events value{}; value.version = PW_VERSION_PROXY_EVENTS;
        value.destroy = nodeDestroyed; return value;
    }();
    pw_proxy_add_listener(proxy, &hold->listener, &events, hold.get());
    m_nodes.push_back(std::move(hold));
    return wait([&] { return WirePlumberGraph::findNodeByName(m_manager, name).has_value(); });
}
void GainNativeGraph::nodeDestroyed(void *data)
{
    auto *hold = static_cast<NodeHold *>(data);
    hold->proxy = nullptr;
    spa_hook_remove(&hold->listener);
}
void GainNativeGraph::moduleDestroyed(void *data)
{
    auto *hold = static_cast<ModuleHold *>(data);
    hold->module = nullptr;
    spa_hook_remove(&hold->listener);
}
pw_impl_module *GainNativeGraph::load(const QByteArray &arguments, OwnedLoopbackLoadHooks hooks)
{
    auto requestedGate = std::move(hooks.admitModule);
    hooks.admitModule = [this, requestedGate] {
        return (!requestedGate || requestedGate())
            && m_runtime.image().bootstrapStillPinned();
    };
    return holdModule(loadOwnedLoopback(m_core, arguments, std::move(hooks)));
}
pw_impl_module *GainNativeGraph::loadStandalone(const QByteArray &arguments)
{
    if (!m_core || !wp_core_is_connected(m_core) || !m_runtime.image().bootstrapStillPinned())
        return nullptr;
    auto *context = wp_core_get_pw_context(m_core);
    if (pw_context_get_object(context, PW_TYPE_INTERFACE_Core) != nullptr) return nullptr;
    return holdModule(pw_context_load_module(context, "libpipewire-module-loopback",
                                            arguments.constData(), nullptr));
}
pw_impl_module *GainNativeGraph::holdModule(pw_impl_module *module)
{
    if (!module) return nullptr;
    // The bootstrap pinned the exact file before the public factory. A
    // different loaded image is a fixture breach: do not attempt unknown
    // borrowed-module teardown. The recipe owns a fresh process group and
    // kills only its remaining private children on any abnormal exit.
    if (!m_runtime.image().loadedModule(module))
        qFatal("qualified-loopback-loaded-image-breach");
    auto hold = std::make_unique<ModuleHold>();
    hold->module = module;
    static const pw_impl_module_events events = [] {
        pw_impl_module_events value{}; value.version = PW_VERSION_IMPL_MODULE_EVENTS;
        value.destroy = moduleDestroyed; return value;
    }();
    pw_impl_module_add_listener(module, &hold->listener, &events, hold.get());
    m_modules.push_back(std::move(hold));
    return module;
}
void GainNativeGraph::destroy(pw_impl_module *module)
{
    for (const auto &hold : m_modules) if (hold->module == module) {
        pw_impl_module_destroy(module); return;
    }
}
bool GainNativeGraph::setGain(const ObservedSendNode &node, ConsoleSendAmplitude amplitude)
{
    // This helper independently requires the same cached object+serial. The
    // caller must first obtain the node from ConsoleSendAdmission::playback.
    const auto current = WirePlumberGraph::findNode(m_manager, node.serial);
    if (!current || reinterpret_cast<quintptr>(current->node) != node.objectIdentity
        || current->boundId != node.boundId) return false;
    return setById(node.boundId, amplitude);
}
bool GainNativeGraph::setById(const quint32 id, const ConsoleSendAmplitude amplitude)
{
    if (!m_mixer || !wp_core_is_connected(m_core) || !amplitude.valid) return false;
    GVariantBuilder dictionary; g_variant_builder_init(&dictionary, G_VARIANT_TYPE_VARDICT);
    GVariantBuilder channels; g_variant_builder_init(&channels, G_VARIANT_TYPE_VARDICT);
    for (int index = 0; index < 2; ++index) {
        GVariantBuilder one; g_variant_builder_init(&one, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&one, "{sv}", "volume",
            g_variant_new_double(consoleMixerScalar(index == 0 ? amplitude.left : amplitude.right)));
        g_variant_builder_add(&channels, "{sv}", index == 0 ? "0" : "1",
                              g_variant_builder_end(&one));
    }
    g_variant_builder_add(&dictionary, "{sv}", "channelVolumes", g_variant_builder_end(&channels));
    g_variant_builder_add(&dictionary, "{sv}", "mute", g_variant_new_boolean(FALSE));
    GVariant *value = g_variant_builder_end(&dictionary);
    gboolean accepted = FALSE;
    g_signal_emit_by_name(m_mixer, "set-volume", id, value, &accepted);
    g_variant_unref(value);
    return accepted != FALSE;
}
std::optional<NativeSendProps> GainNativeGraph::props(const ObservedSendNode &node) const
{
    const auto current = WirePlumberGraph::findNode(m_manager, node.serial);
    if (!current || reinterpret_cast<quintptr>(current->node) != node.objectIdentity) return std::nullopt;
    WpIterator *params = wp_pipewire_object_enum_params_sync(
        WP_PIPEWIRE_OBJECT(current->node), "Props", nullptr);
    if (!params) return std::nullopt;
    std::optional<NativeSendProps> result;
    GValue item = G_VALUE_INIT;
    while (wp_iterator_next(params, &item)) {
        const auto *pod = wp_spa_pod_get_spa_pod(
            static_cast<WpSpaPod *>(g_value_get_boxed(&item)));
        const spa_pod_prop *volumes = pod ? spa_pod_find_prop(pod, nullptr, SPA_PROP_channelVolumes) : nullptr;
        const spa_pod_prop *mute = pod ? spa_pod_find_prop(pod, nullptr, SPA_PROP_mute) : nullptr;
        uint32_t count = 0, size = 0, type = 0;
        auto *array = volumes ? static_cast<float *>(spa_pod_get_array_full(
            &volumes->value, &count, &size, &type)) : nullptr;
        bool muted = true;
        if (array && count == 2 && size == sizeof(float) && type == SPA_TYPE_Float
            && mute && spa_pod_get_bool(&mute->value, &muted) >= 0
            && std::isfinite(array[0]) && std::isfinite(array[1])) {
            result = NativeSendProps{{array[0], array[1]}, muted};
        }
        g_value_unset(&item);
    }
    wp_iterator_unref(params);
    return result;
}
WirePlumberGraph::BuildResult GainNativeGraph::snapshot(const ConsoleSendAdmission *admission) const
{
    const Capabilities capabilities = Capability::SetVolume | Capability::SetMute
        | Capability::SetChannelVolumes | Capability::ManageVirtualDevices;
    return WirePlumberGraph::buildSnapshot(m_manager, m_mixer, m_defaults, 1, 1,
                                          capabilities, {}, admission);
}
void GainNativeGraph::disconnect()
{
    if (m_core) wp_core_disconnect(m_core);
}
} // namespace QindaQt::Audio::Test

// SPDX-License-Identifier: GPL-3.0-or-later

#include "console_endpoints_p.h"
#include "wireplumber_graph_p.h"
#include "wireplumber_worker_p.h"

#include <QtCore/QDebug>
#include <QtCore/QSet>

#include <pipewire/pipewire.h>
#include <pipewire/impl-module.h>

#include <algorithm>
#include <utility>

namespace QindaQt::Audio
{
namespace
{

// AGENT-NOTE: a strip's sink lingers by design, so dropping this service's
// proxy never takes it down, but the DAEMON can still destroy the node (a
// `pw-cli destroy`, a policy). The proxy then receives `removed`, and the
// record that guards against duplicate creation must go with it, or the
// strip has no sink until the next reconnect. Owned by the proxy listener and
// freed with the proxy.
struct EndpointProxyHold {
    WirePlumberWorker *worker = nullptr;
    void *proxy = nullptr;
    bool bound = false;
    spa_hook listener{};
};

void onEndpointProxyDestroy(void *data)
{
    auto *const hold = static_cast<EndpointProxyHold *>(data);
    spa_hook_remove(&hold->listener);
    delete hold;
}

void onEndpointProxyBound(void *data, const uint32_t globalId)
{
    Q_UNUSED(globalId)
    static_cast<EndpointProxyHold *>(data)->bound = true;
}

void onEndpointProxyRemoved(void *data)
{
    const auto *const hold = static_cast<EndpointProxyHold *>(data);
    hold->worker->endpointProxyRemoved(hold->proxy, hold->bound);
}

constexpr pw_proxy_events kEndpointProxyEvents = {
    .version = PW_VERSION_PROXY_EVENTS,
    .destroy = onEndpointProxyDestroy,
    .bound = onEndpointProxyBound,
    .removed = onEndpointProxyRemoved,
    .done = nullptr,
    .error = nullptr,
    .bound_props = nullptr,
};

// AGENT-GUARD: recovery is spaced. Something that destroys the sink as soon
// as it appears must not turn into a create/destroy loop at idle speed.
constexpr gint64 kEndpointRecoverySpacingUs = 2 * G_USEC_PER_SEC;

void watchEndpointProxy(WirePlumberWorker *const worker, struct pw_proxy *const proxy)
{
    auto *const hold = new EndpointProxyHold{
        .worker = worker, .proxy = proxy, .bound = false, .listener = {}};
    pw_proxy_add_listener(proxy, &hold->listener, &kEndpointProxyEvents, hold);
}

} // namespace

void WirePlumberWorker::applyConsoleEndpoints(QList<BackendConsoleEndpoint> endpoints)
{
    invoke([this, endpoints = std::move(endpoints)] {
        applyConsoleEndpointsOnWorker(endpoints);
    });
}

void WirePlumberWorker::applyConsoleEndpointsOnWorker(
    const QList<BackendConsoleEndpoint> &endpoints)
{
    m_declaredEndpoints = endpoints;
    if (m_core == nullptr || m_manager == nullptr || m_daemonSerial == 0) {
        return;
    }
    struct pw_context *const context = wp_core_get_pw_context(m_core);
    struct pw_core *const core = wp_core_get_pw_core(m_core);
    if (context == nullptr || core == nullptr) {
        return;
    }
    // AGENT-GUARD: a physical bus's pre-rack sink exists only while its rack
    // does. Strip sinks and virtual-bus sinks linger by design; this one was
    // created for a rack the user has now switched off, and left behind it
    // would sit in every device picker as a sink nothing feeds. It is
    // destroyed through the registry, since a lingering object outlives the
    // proxy that made it.
    QSet<QString> wantedBusSinks;
    for (const BackendConsoleEndpoint &endpoint : endpoints) {
        if (endpoint.isBus) {
            wantedBusSinks.insert(ConsoleEndpoints::busSinkNodeName(endpoint.consoleId));
        }
    }
    // Driven by the GRAPH, not by this run's proxies: the leftover may have
    // been made by a previous run of the service.
    const QString busPrefix =
        QLatin1String(ConsoleEndpoints::kConsoleNodeNamePrefix) + QStringLiteral("bus.");
    for (const QString &name : WirePlumberGraph::nodeNamesWithPrefix(m_manager, busPrefix)) {
        // Only the sink itself: its `.source`, `.rack.*` and stream siblings
        // are modules that die with their owner.
        if (name.mid(busPrefix.size()).contains(QLatin1Char('.'))
            && !name.mid(busPrefix.size()).startsWith(QStringLiteral("a"))) {
            continue;
        }
        const QString tail = name.mid(busPrefix.size());
        if (tail.count(QLatin1Char('.')) != 0 || wantedBusSinks.contains(name)) {
            continue;
        }
        if (auto node = WirePlumberGraph::findNodeByName(m_manager, name); node.has_value()) {
            wp_global_proxy_request_destroy(WP_GLOBAL_PROXY(node->node));
        }
        const auto proxy = m_endpointProxies.find(name.toStdString());
        if (proxy != m_endpointProxies.end()) {
            std::erase(m_removedEndpointProxies, proxy->second);
            pw_proxy_destroy(static_cast<struct pw_proxy *>(proxy->second));
            m_endpointProxies.erase(proxy);
        }
    }
    // AGENT-GUARD: a refused virtual-bus loopback is attempted once per
    // declaration, not on every rebuild (module_refusals_p.h).
    std::unordered_map<std::string, QByteArray> busLoopbacks;
    for (const BackendConsoleEndpoint &endpoint : endpoints) {
        if (endpoint.isBus && !endpoint.physicalBusSink && !endpoint.consoleId.isEmpty()) {
            busLoopbacks.emplace(
                ConsoleEndpoints::busSinkNodeName(endpoint.consoleId).toStdString(),
                ConsoleEndpoints::busModuleArguments(endpoint.consoleId, endpoint.description));
        }
    }
    m_refusedEndpoints.retainDeclared(busLoopbacks);
    for (const BackendConsoleEndpoint &endpoint : endpoints) {
        if (endpoint.consoleId.isEmpty()) {
            continue;
        }
        const QString sinkName = endpoint.isBus
            ? ConsoleEndpoints::busSinkNodeName(endpoint.consoleId)
            : ConsoleEndpoints::stripSinkNodeName(endpoint.consoleId);
        const std::string key = sinkName.toStdString();
        // Already requested by this worker, or already in the graph from
        // another owner - a previous run of the service, or the shipped daemon
        // configuration: nothing to do. A second copy would give applications
        // two identical devices to choose between.
        if (m_endpointModules.count(key) != 0 || m_endpointProxies.count(key) != 0
            || WirePlumberGraph::findNodeByName(m_manager, sinkName).has_value()) {
            continue;
        }
        if (endpoint.isBus && !endpoint.physicalBusSink) {
            const QByteArray arguments = ConsoleEndpoints::busModuleArguments(
                endpoint.consoleId, endpoint.description);
            if (arguments.isEmpty() || m_refusedEndpoints.refused(key, arguments)) {
                continue;
            }
            struct pw_impl_module *const module = pw_context_load_module(
                context, "libpipewire-module-loopback", arguments.constData(),
                nullptr);
            if (module != nullptr) {
                // AGENT-GUARD: PipeWire may destroy this module itself. The
                // live record must disappear with it, or rebuild skips the
                // missing bus and shutdown destroys a stale pointer.
                watchModule(ModuleKind::Endpoint, key, module);
                m_endpointModules.emplace(key, module);
            } else {
                m_refusedEndpoints.remember(key, arguments);
                qWarning().noquote() << QStringLiteral("Audio1: PipeWire refused console bus %1")
                                            .arg(endpoint.consoleId);
            }
            continue;
        }
        // A strip's sink, or a physical bus's pre-rack sink: the same null sink
        // under the endpoint's own name.
        const auto properties = endpoint.physicalBusSink
            ? ConsoleEndpoints::busSinkProperties(endpoint.consoleId, endpoint.description)
            : ConsoleEndpoints::stripSinkProperties(endpoint.consoleId, endpoint.description);
        if (properties.isEmpty()) {
            continue;
        }
        struct pw_properties *const props = pw_properties_new(nullptr, nullptr);
        for (const auto &[name, value] : properties) {
            pw_properties_set(props, name.constData(), value.constData());
        }
        // A raw proxy on purpose: see console_endpoints_p.h.
        struct pw_proxy *const proxy = static_cast<struct pw_proxy *>(
            pw_core_create_object(core, "adapter", PW_TYPE_INTERFACE_Node,
                                  PW_VERSION_NODE, &props->dict, 0));
        pw_properties_free(props);
        if (proxy != nullptr) {
            watchEndpointProxy(this, proxy);
            m_endpointProxies.emplace(key, proxy);
        }
        // A refused creation is retried on the next rebuild: only a local
        // failure returns no proxy, and the daemon's answer arrives later.
    }
}

void WirePlumberWorker::endpointProxyRemoved(void *const proxy, const bool bound)
{
    // An unbound proxy is a creation the daemon refused: its record stays, so
    // the sink is not asked for again on every rebuild (until the next core).
    if (!bound || m_stopping || m_context == nullptr) {
        return;
    }
    m_removedEndpointProxies.push_back(proxy);
    if (m_endpointRecoverySource != nullptr) {
        return;
    }
    // Deferred: this runs inside the protocol dispatch that delivered the
    // event, and a rebuild loads modules and creates objects.
    const gint64 elapsed = g_get_monotonic_time() - m_lastEndpointRecovery;
    const gint64 waitUs = m_lastEndpointRecovery == 0
        ? 0
        : std::clamp<gint64>(kEndpointRecoverySpacingUs - elapsed, 0,
                             kEndpointRecoverySpacingUs);
    GSource *const source = g_timeout_source_new(static_cast<guint>(waitUs / 1000));
    g_source_set_callback(source, dispatchEndpointRecovery, this, nullptr);
    g_source_attach(source, m_context);
    m_endpointRecoverySource = source;
}

gboolean WirePlumberWorker::dispatchEndpointRecovery(gpointer data)
{
    auto *const self = static_cast<WirePlumberWorker *>(data);
    GSource *const source = std::exchange(self->m_endpointRecoverySource, nullptr);
    self->recoverRemovedEndpoints();
    if (source != nullptr) {
        g_source_unref(source);
    }
    return G_SOURCE_REMOVE;
}

void WirePlumberWorker::recoverRemovedEndpoints()
{
    std::vector<void *> removed;
    removed.swap(m_removedEndpointProxies);
    for (void *const proxy : removed) {
        const auto record = std::find_if(m_endpointProxies.begin(), m_endpointProxies.end(),
                                         [proxy](const auto &entry) {
                                             return entry.second == proxy;
                                         });
        if (record != m_endpointProxies.end()) {
            qWarning().noquote()
                << QStringLiteral("Audio1: PipeWire removed console endpoint %1; recreating it")
                       .arg(QString::fromStdString(record->first));
            m_endpointProxies.erase(record);
            pw_proxy_destroy(static_cast<struct pw_proxy *>(proxy));
        }
    }
    m_lastEndpointRecovery = g_get_monotonic_time();
    // A sink whose node the object manager still lists is created by the
    // rebuild its removal triggers; this one covers the other order.
    rebuild();
}

void WirePlumberWorker::unloadAllEndpoints()
{
    // Recovery belongs to this core: its proxies are destroyed below.
    if (m_endpointRecoverySource != nullptr) {
        g_source_destroy(m_endpointRecoverySource);
        g_source_unref(std::exchange(m_endpointRecoverySource, nullptr));
    }
    m_removedEndpointProxies.clear();
    m_lastEndpointRecovery = 0;
    // A replaced core earns every refused bus loopback one fresh attempt.
    m_refusedEndpoints.clear();
    // A destroy callback removes its own entry; take the map before walking.
    std::unordered_map<std::string, void *> loaded;
    loaded.swap(m_endpointModules);
    for (auto &[name, module] : loaded) {
        pw_impl_module_destroy(static_cast<struct pw_impl_module *>(module));
    }
    // The sinks themselves linger by design; only the proxies go with the core.
    for (auto &[name, proxy] : m_endpointProxies) {
        pw_proxy_destroy(static_cast<struct pw_proxy *>(proxy));
    }
    m_endpointProxies.clear();
}

} // namespace QindaQt::Audio

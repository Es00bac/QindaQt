// SPDX-License-Identifier: GPL-3.0-or-later

#include "console_endpoints_p.h"
#include "wireplumber_graph_p.h"
#include "wireplumber_worker_p.h"

#include <pipewire/pipewire.h>
#include <pipewire/impl-module.h>

#include <utility>

namespace QindaQt::Audio
{

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
        if (endpoint.isBus) {
            const QByteArray arguments = ConsoleEndpoints::busModuleArguments(
                endpoint.consoleId, endpoint.description);
            if (arguments.isEmpty()) {
                continue;
            }
            struct pw_impl_module *const module = pw_context_load_module(
                context, "libpipewire-module-loopback", arguments.constData(),
                nullptr);
            if (module != nullptr) {
                m_endpointModules.emplace(key, module);
            }
            continue;
        }
        const auto properties = ConsoleEndpoints::stripSinkProperties(
            endpoint.consoleId, endpoint.description);
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
            m_endpointProxies.emplace(key, proxy);
        }
        // A refused creation is retried on the next rebuild, like a send.
    }
}

void WirePlumberWorker::unloadAllEndpoints()
{
    for (auto &[name, module] : m_endpointModules) {
        pw_impl_module_destroy(static_cast<struct pw_impl_module *>(module));
    }
    m_endpointModules.clear();
    // The sinks themselves linger by design; only the proxies go with the core.
    for (auto &[name, proxy] : m_endpointProxies) {
        pw_proxy_destroy(static_cast<struct pw_proxy *>(proxy));
    }
    m_endpointProxies.clear();
}

} // namespace QindaQt::Audio

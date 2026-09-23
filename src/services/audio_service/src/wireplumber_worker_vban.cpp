// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include "wireplumber_graph_p.h"

#include <pipewire/impl-module.h>

#include <algorithm>
#include <utility>

namespace QindaQt::Audio
{
namespace {

// A VBAN route is locally active only after the selected PipeWire graph edge
// exists. Node registration alone happens before WirePlumber linking, and was
// enough to make Settings briefly claim an active route with no audio path.
bool connectedEdge(WpObjectManager *manager, guint32 outputNode, guint32 inputNode)
{
    if (outputNode == 0 || inputNode == 0) return false;
    WpIterator *links = wp_object_manager_new_filtered_iterator(manager, WP_TYPE_LINK, nullptr);
    GValue value = G_VALUE_INIT;
    bool found = false;
    while (wp_iterator_next(links, &value)) {
        auto *link = WP_LINK(g_value_get_object(&value));
        guint32 output = 0;
        guint32 input = 0;
        wp_link_get_linked_object_ids(link, &output, nullptr, &input, nullptr);
        const WpLinkState state = wp_link_get_state(link, nullptr);
        found = output == outputNode && input == inputNode
            && state >= WP_LINK_STATE_PAUSED;
        g_value_unset(&value);
        if (found) break;
    }
    wp_iterator_unref(links);
    return found;
}

} // namespace

void WirePlumberWorker::applyVban(QList<BackendVbanStream> streams)
{
    invoke([this, streams = std::move(streams)] { applyVbanOnWorker(streams); });
}

void WirePlumberWorker::applyVbanOnWorker(const QList<BackendVbanStream> &streams)
{
    m_declaredVban = streams;
    if (m_core == nullptr || m_manager == nullptr) {
        publishVbanRunning();
        return;
    }
    struct pw_context *const context = wp_core_get_pw_context(m_core);
    if (context == nullptr) {
        publishVbanRunning();
        return;
    }
    // Teardown happens before replacement, so a changed authorized source or
    // output never overlaps an old open receiver on the same UDP port.
    for (auto it = m_vbanRuns.begin(); it != m_vbanRuns.end();) {
        const BackendVbanStream *wanted = nullptr;
        for (const BackendVbanStream &stream : streams) {
            if (stream.name.toStdString() == it->first) wanted = &stream;
        }
        const bool routeLost = wanted != nullptr && !wanted->outgoing
            && !m_vbanRouteModules.contains(it->first);
        if (wanted == nullptr || *wanted != it->second.declared || routeLost) {
            const auto route = m_vbanRouteModules.find(it->first);
            if (route != m_vbanRouteModules.end()) {
                destroyModuleLater(route->second.module);
                m_vbanRouteModules.erase(route);
            }
            it = m_vbanRuns.erase(it);
        } else {
            ++it;
        }
    }
    for (const BackendVbanStream &stream : streams) {
        const std::string key = stream.name.toStdString();
        if (m_vbanRuns.contains(key)) continue;
        const QString target = nodeNameForHandle(stream.target);
        if (target.isEmpty()) continue; // offline target: never fall back
        VbanRun run;
        run.declared = stream;
        bool started = false;
        if (stream.outgoing) {
            run.sender = std::make_unique<VbanSender>();
            started = run.sender->start(context, target,
                                        stream.name, stream.host,
                                        static_cast<quint16>(stream.port));
        } else {
            const QByteArray arguments = vbanRouteArguments(stream.name, target);
            if (arguments.isEmpty()) continue;
            run.receiver = std::make_unique<VbanReceiver>();
            if (!run.receiver->start(context, stream.name,
                                     static_cast<quint16>(stream.port), stream.host))
                continue;
            struct pw_impl_module *const module = pw_context_load_module(
                context, "libpipewire-module-loopback", arguments.constData(), nullptr);
            if (module != nullptr) {
                watchModule(ModuleKind::VbanRoute, key, module);
                m_vbanRouteModules.emplace(key, LoadedModule{.module = module,
                                                               .arguments = arguments});
                started = true;
            }
        }
        if (started) m_vbanRuns.emplace(key, std::move(run));
    }
    publishVbanRunning();
}

void WirePlumberWorker::publishVbanRunning()
{
    QList<BackendVbanStream> running;
    if (m_manager != nullptr) {
        for (const auto &[name, run] : m_vbanRuns) {
            const QString streamName = QString::fromStdString(name);
            const QString targetName = nodeNameForHandle(run.declared.target);
            if (targetName.isEmpty()) continue;
            const auto target = WirePlumberGraph::findNodeByName(m_manager, targetName);
            if (!target.has_value()) continue;
            if (run.declared.outgoing) {
                const auto capture = WirePlumberGraph::findNodeByName(
                    m_manager, QStringLiteral("qindaqt.vban.send.") + streamName);
                if (run.sender && run.sender->running() && capture.has_value()
                    && connectedEdge(m_manager, target->boundId, capture->boundId))
                    running.append(run.declared);
            } else {
                const auto source = WirePlumberGraph::findNodeByName(
                    m_manager, vbanSourceNodeName(streamName));
                const auto routeCapture = WirePlumberGraph::findNodeByName(
                    m_manager, vbanRouteNodeName(streamName) + QStringLiteral(".capture"));
                const auto routePlayback = WirePlumberGraph::findNodeByName(
                    m_manager, vbanRouteNodeName(streamName));
                if (run.receiver && run.receiver->running()
                    && m_vbanRouteModules.contains(name) && source.has_value()
                    && routeCapture.has_value() && routePlayback.has_value()
                    && connectedEdge(m_manager, source->boundId, routeCapture->boundId)
                    && connectedEdge(m_manager, routePlayback->boundId, target->boundId))
                    running.append(run.declared);
            }
        }
    }
    std::sort(running.begin(), running.end(),
              [](const BackendVbanStream &a, const BackendVbanStream &b) {
                  return a.name < b.name;
              });
    if (running == m_reportedVbanRunning) return;
    m_reportedVbanRunning = running;
    if (m_vbanRunningCallback) m_vbanRunningCallback(running);
}

void WirePlumberWorker::stopAllVban()
{
    // cleanupCore has stopped all dispatch and destroys current modules in
    // this context before the receiver streams and PipeWire core disappear.
    std::unordered_map<std::string, LoadedModule> routes;
    routes.swap(m_vbanRouteModules);
    for (auto &[name, route] : routes)
        pw_impl_module_destroy(static_cast<struct pw_impl_module *>(route.module));
    m_vbanRuns.clear();
    publishVbanRunning();
}

} // namespace QindaQt::Audio

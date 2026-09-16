// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_graph_p.h"
#include "wireplumber_routing_p.h"
#include "wireplumber_worker_p.h"

#include <qindaqt/services/audio_protocol/audio_gain.h>

#include <pipewire/pipewire.h>
// pw_context_load_module / pw_impl_module_destroy live in the impl API, which
// pipewire.h does not pull in.
#include <pipewire/impl-module.h>

#include <utility>

namespace QindaQt::Audio
{

void WirePlumberWorker::applyRouting(QList<BackendRoutingEdge> edges)
{
    invoke([this, edges = std::move(edges)] { applyRoutingOnWorker(edges); });
}

QString WirePlumberWorker::nodeNameForHandle(const Handle &handle) const
{
    if (handle.epoch != m_epoch || !handle.isValid()) {
        return {};
    }
    const auto node = WirePlumberGraph::findNode(m_manager, handle.serial);
    return node.has_value() ? node->nodeName : QString{};
}

void WirePlumberWorker::applyRoutingOnWorker(const QList<BackendRoutingEdge> &edges)
{
    m_declaredRouting = edges;
    if (m_core == nullptr || m_manager == nullptr) {
        return;
    }
    struct pw_context *const context = wp_core_get_pw_context(m_core);
    if (context == nullptr) {
        return;
    }

    // Everything the console wants that can actually be carried right now: an
    // edge whose strip or bus has no live node is simply not buildable yet and
    // is left out rather than approximated.
    std::unordered_map<std::string, QByteArray> wanted;
    for (const BackendRoutingEdge &edge : edges) {
        bool sourceIsSink = edge.sourceIsSink;
        const QString source = stripReadNode(edge.stripId, edge.source, &sourceIsSink);
        const QString target = nodeNameForHandle(edge.target);
        if (source.isEmpty() || target.isEmpty()) {
            continue;
        }
        const QByteArray arguments = routingModuleArguments(
            edge.stripId, edge.busId, source, target, sourceIsSink,
            edge.audible ? linearFromGainDb(edge.gainDb) : 0.0);
        if (arguments.isEmpty()) {
            continue;
        }
        wanted.emplace(routingNodeName(edge.stripId, edge.busId).toStdString(),
                       arguments);
    }

    // AGENT-GUARD: unload before load. A send whose endpoints changed must have
    // its old module destroyed first, or two loopbacks briefly carry the same
    // cell and the user hears it at double level.
    for (auto it = m_routingModules.begin(); it != m_routingModules.end();) {
        const auto want = wanted.find(it->first);
        // Gone, or here with different endpoints: a send whose source moved
        // (a re-pin, a rack starting) is a different send under the same name.
        if (want == wanted.end() || want->second != it->second.arguments) {
            destroyModuleLater(it->second.module);
            it = m_routingModules.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto &[name, arguments] : wanted) {
        if (m_routingModules.find(name) != m_routingModules.end()) {
            continue;
        }
        struct pw_impl_module *const module = pw_context_load_module(
            context, "libpipewire-module-loopback", arguments.constData(), nullptr);
        if (module == nullptr) {
            // A refused module is reported through the snapshot's diagnostic on
            // the next publication rather than failing the whole console: the
            // rest of the user's routing must keep working.
            continue;
        }
        watchModule(ModuleKind::Send, name, module);
        m_routingModules.emplace(name, LoadedModule{.module = module, .arguments = arguments});
    }
    applySendVolumes();
}

void WirePlumberWorker::applySendVolumes()
{
    if (m_mixer == nullptr || m_manager == nullptr) {
        return;
    }
    // AGENT-CONTRACT: the send's gain is the VOLUME of its loopback playback
    // node, which is what makes every matrix cell independently adjustable.
    // It is re-applied on every graph change rather than only at load, because
    // the node does not exist until PipeWire has finished creating the module.
    for (const BackendRoutingEdge &edge : m_declaredRouting) {
        const QString name = routingNodeName(edge.stripId, edge.busId);
        if (m_routingModules.find(name.toStdString()) == m_routingModules.end()) {
            continue;
        }
        const auto node = WirePlumberGraph::findNodeByName(m_manager, name);
        if (!node.has_value()) {
            continue;
        }
        // A silenced edge is carried at zero rather than removed, so unmuting
        // or unsoloing is instant instead of a graph rebuild.
        const double linear = edge.audible ? linearFromGainDb(edge.gainDb) : 0.0;
        // Pan is the two playback channels at different gains (ADR-0177). The
        // send is always stereo (see routingModuleArguments), so the keys are
        // exactly "0" and "1".
        const PanGains pan = panGains(edge.pan);
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        // AGENT-CONTRACT: mixer-api set-volume takes per-channel state as
        // {channelVolumes: a{sv}} keyed by decimal channel index with nested
        // {"volume": d}; a plain "volume" would set both channels alike.
        GVariantBuilder channels;
        g_variant_builder_init(&channels, G_VARIANT_TYPE("a{sv}"));
        const double perChannel[2] = {linear * pan.left, linear * pan.right};
        for (int index = 0; index < 2; ++index) {
            GVariantBuilder one;
            g_variant_builder_init(&one, G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add(&one, "{sv}", "volume",
                                  g_variant_new_double(perChannel[index]));
            g_variant_builder_add(&channels, "{sv}", index == 0 ? "0" : "1",
                                  g_variant_builder_end(&one));
        }
        g_variant_builder_add(&builder, "{sv}", "channelVolumes",
                              g_variant_builder_end(&channels));
        GVariant *dictionary = g_variant_builder_end(&builder);
        gboolean result = FALSE;
        g_signal_emit_by_name(m_mixer, "set-volume", node->boundId, dictionary,
                              &result);
        g_variant_unref(dictionary);
    }
}

void WirePlumberWorker::unloadAllRouting()
{
    // Take the map first: each destroy fires the module's destroy event, which
    // would otherwise erase from the map being walked.
    std::unordered_map<std::string, LoadedModule> loaded;
    loaded.swap(m_routingModules);
    for (auto &[name, entry] : loaded) {
        pw_impl_module_destroy(static_cast<struct pw_impl_module *>(entry.module));
    }
}

} // namespace QindaQt::Audio

// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_graph_p.h"
#include "wireplumber_processing_p.h"
#include "wireplumber_worker_p.h"

#include <QtCore/QtGlobal>

#include <pipewire/pipewire.h>
#include <pipewire/impl-module.h>

#include <cstdio>
#include <utility>

namespace QindaQt::Audio
{
namespace {

// QINDAQT_AUDIO_DEBUG_MODULES=1 prints every module argument the worker hands
// PipeWire and whether it was accepted. PipeWire itself refuses a malformed
// filter-chain with nothing on stderr at its default log level, so without
// this a wrong control name is invisible.
bool moduleDebug()
{
    static const bool enabled = !qEnvironmentVariableIsEmpty("QINDAQT_AUDIO_DEBUG_MODULES");
    return enabled;
}

} // namespace

void WirePlumberWorker::applyProcessing(QList<BackendProcessingChain> chains)
{
    invoke([this, chains = std::move(chains)] { applyProcessingOnWorker(chains); });
}

void WirePlumberWorker::applyProcessingOnWorker(const QList<BackendProcessingChain> &chains)
{
    m_declaredProcessing = chains;
    if (m_core == nullptr || m_manager == nullptr) {
        return;
    }
    struct pw_context *const context = wp_core_get_pw_context(m_core);
    if (context == nullptr) {
        return;
    }
    // What should be running: strip id -> module argument. A rack whose
    // device is not in the graph right now is simply not buildable yet.
    std::unordered_map<std::string, QByteArray> wanted;
    for (const BackendProcessingChain &chain : chains) {
        const QString source = nodeNameForHandle(chain.source);
        if (source.isEmpty()) {
            continue;
        }
        const QByteArray arguments = processingModuleArguments(
            chain.stripId, source, chain.sourceIsSink, chain.processing);
        if (!arguments.isEmpty()) {
            wanted.emplace(chain.stripId.toStdString(), arguments);
        }
    }
    // AGENT-GUARD: unload before load, and a CHANGED rack is unload-then-load.
    // Two chains on one strip would double its audio into every send.
    for (auto it = m_processingModules.begin(); it != m_processingModules.end();) {
        const auto want = wanted.find(it->first);
        if (want == wanted.end() || want->second != it->second.arguments) {
            destroyModuleLater(it->second.module);
            it = m_processingModules.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto &[stripId, arguments] : wanted) {
        if (m_processingModules.find(stripId) != m_processingModules.end()) {
            continue;
        }
        struct pw_impl_module *const module = pw_context_load_module(
            context, "libpipewire-module-filter-chain", arguments.constData(), nullptr);
        if (moduleDebug()) {
            fprintf(stderr, "qindaqt-audio: filter-chain for %s -> %s\n  %s\n", stripId.c_str(),
                    module != nullptr ? "loaded" : "REFUSED", arguments.constData());
        }
        if (module == nullptr) {
            // A refused chain - a missing plugin, most likely - leaves the
            // strip unprocessed rather than silent: its sends keep reading
            // the device. Retried on the next rebuild.
            continue;
        }
        watchModule(ModuleKind::Chain, stripId, module);
        m_processingModules.emplace(stripId,
                                    LoadedModule{.module = module, .arguments = arguments});
    }
}

void WirePlumberWorker::unloadAllProcessing()
{
    std::unordered_map<std::string, LoadedModule> loaded;
    loaded.swap(m_processingModules);
    for (auto &[stripId, entry] : loaded) {
        pw_impl_module_destroy(static_cast<struct pw_impl_module *>(entry.module));
    }
}

QString WirePlumberWorker::stripReadNode(const QString &stripId, const Handle &device,
                                         bool *readsSink) const
{
    // AGENT-CONTRACT: a running rack interposes itself. Every send and the
    // meter read the processed sink's monitor, so what the user hears and
    // sees IS the processed signal; a strip whose rack is not (yet) running
    // reads its device exactly as before, so a missing plugin degrades to
    // "unprocessed", never to "silent".
    // AGENT-GUARD: only once the processed node actually EXISTS. A chain's
    // nodes appear a graph event after its module loads; a send pointed at a
    // name the graph does not have yet is autoconnected by PipeWire to the
    // default device instead and never re-linked. Until then the strip reads
    // its device unprocessed; the rebuild that announces the node moves the
    // sends and the meter over - one event later, and audibly not "silent".
    if (m_processingModules.find(stripId.toStdString()) != m_processingModules.end()
        && WirePlumberGraph::findNodeByName(m_manager, processedNodeName(stripId))
               .has_value()) {
        // The processed node is a virtual source, whatever the device was.
        if (readsSink != nullptr) {
            *readsSink = false;
        }
        return processedNodeName(stripId);
    }
    return nodeNameForHandle(device);
}

} // namespace QindaQt::Audio

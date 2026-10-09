// SPDX-License-Identifier: GPL-3.0-or-later
#include "wireplumber_worker_p.h"
#include "wireplumber_context_core_p.h"
#include "wireplumber_graph_p.h"
#include "wireplumber_send_image_p.h"
#include "wireplumber_send_nodes_p.h"

#include <pipewire/impl-module.h>

namespace QindaQt::Audio
{
namespace {
bool writeSend(WpPlugin *mixer, WpCore *core, WpObjectManager *manager,
               quint64 generation, const ObservedSendNode &node,
               const ConsoleSendAmplitude amplitude)
{
    const auto producer = sendProducerIdentity(core, manager, generation);
    if (!mixer || !amplitude.valid || !producer.valid()
        || producer.clientId != node.clientId || !observedSendNodes(manager).contains(node)) {
        return false;
    }
    GVariantBuilder dictionary, channels;
    g_variant_builder_init(&dictionary, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_init(&channels, G_VARIANT_TYPE_VARDICT);
    const double perChannel[] = {amplitude.left, amplitude.right};
    for (int channel = 0; channel < 2; ++channel) {
        GVariantBuilder value;
        g_variant_builder_init(&value, G_VARIANT_TYPE_VARDICT);
        // AGENT-CONTRACT: mixer-api scale1 is cubic. Only the admitted private
        // send inverts that law; ordinary public 0..1 controls stay unchanged.
        g_variant_builder_add(&value, "{sv}", "volume",
            g_variant_new_double(consoleMixerScalar(perChannel[channel])));
        g_variant_builder_add(&channels, "{sv}", channel == 0 ? "0" : "1",
            g_variant_builder_end(&value));
    }
    g_variant_builder_add(&dictionary, "{sv}", "channelVolumes", g_variant_builder_end(&channels));
    g_variant_builder_add(&dictionary, "{sv}", "mute",
        g_variant_new_boolean(amplitude.left == 0.0 && amplitude.right == 0.0));
    GVariant *values = g_variant_builder_end(&dictionary);
    gboolean accepted = FALSE;
    g_signal_emit_by_name(mixer, "set-volume", node.boundId, values, &accepted);
    g_variant_unref(values);
    return accepted != FALSE;
}
}

ConsoleSendRuntime WirePlumberWorker::sendRuntime()
{
    return {
        .endpoints = [this](const BackendRoutingEdge &edge) {
            bool sink = edge.sourceIsSink;
            const QString source = stripReadNode(edge.stripId, edge.source, &sink);
            return SendEndpoints{source, busWriteNode(edge.busId, edge.target), sink};
        },
        .producer = [this] { return sendProducerIdentity(m_core, m_manager, m_workerRun); },
        .nodes = [this] { return observedSendNodes(m_manager); },
        .load = [this](const QString &key, const QByteArray &arguments) -> void * {
            const ConsoleSendImage image;
            auto *module = loadOwnedLoopback(m_core, arguments,
                {.admitModule = [&image] { return image.admitted(); }, .beforeLoad = {}});
            if (!module) return nullptr;
            // Watch even a refused post-load image: PipeWire can destroy it
            // before the deferred cleanup, which must remove the queued handle.
            watchModule(ModuleKind::Send, key.toStdString(), module);
            if (!image.loaded(module)) {
                // Never release quiet Props from a mismatched loaded image.
                destroyModuleLater(module);
                return nullptr;
            }
            return module;
        },
        .write = [this](const ObservedSendNode &node, const ConsoleSendAmplitude amplitude) {
            return writeSend(m_mixer, m_core, m_manager, m_workerRun, node, amplitude);
        },
        .retire = [this](void *module) { destroyModuleLater(module); },
        .destroy = [](void *module) { pw_impl_module_destroy(static_cast<pw_impl_module *>(module)); },
    };
}
} // namespace QindaQt::Audio

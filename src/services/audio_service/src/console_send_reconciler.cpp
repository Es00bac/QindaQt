// SPDX-License-Identifier: GPL-3.0-or-later
#include "console_send_reconciler_p.h"
#include "wireplumber_routing_p.h"
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <algorithm>

namespace QindaQt::Audio
{
namespace {
ConsoleSendAmplitude amplitude(const BackendRoutingEdge &edge)
{
    return consoleSendAmplitude({.stripDb = edge.stripGainDb, .sendDb = edge.gainDb,
        .busDb = edge.busGainDb, .pan = edge.pan, .audible = edge.audible});
}
}

void ConsoleSendReconciler::quiet(const Send &send, const ConsoleSendRuntime &runtime,
                                 const QList<ObservedSendNode> &observed)
{
    // AGENT-GUARD: retirement cannot use a name or recycled bound ID to mute
    // another application. A formerly admitted tuple may only be quieted when
    // that exact current object is still observed; otherwise destroy our module.
    if (send.lastPlayback && observed.contains(*send.lastPlayback)) {
        (void)runtime.write(*send.lastPlayback, {.left = 0.0, .right = 0.0, .valid = true});
    }
}

void ConsoleSendReconciler::retire(const QString &key, const ConsoleSendRuntime &runtime,
                                  const QList<ObservedSendNode> &observed)
{
    const Send send = m_active.take(key);
    quiet(send, runtime, observed);
    m_admission.retire(send.declaration.instance);
    m_retiring.insert(send.module, key);
    runtime.retire(send.module);
}

void ConsoleSendReconciler::reconcile(const QList<BackendRoutingEdge> &edges,
                                     const ConsoleSendRuntime &runtime)
{
    m_refused = false;
    const SendProducerIdentity producer = runtime.producer();
    const QList<ObservedSendNode> observed = runtime.nodes();
    if (producer != m_producer || !producer.valid()) {
        // The actual module handle is still ours to retire after authority
        // loss; node properties cannot authorize a final write in that lineage.
        for (const QString &key : m_active.keys()) retire(key, runtime, {});
        m_admission.reset(producer);
        m_producer = producer;
    }
    if (!producer.valid()) {
        m_refused = !edges.isEmpty();
        return;
    }
    m_admission.observe(producer, observed);
    const auto invalid = m_admission.invalidInstances();
    for (const QString &key : m_active.keys()) {
        if (invalid.contains(m_active.value(key).declaration.instance)) {
            retire(key, runtime, observed);
            m_refused = true;
        }
    }

    QHash<QString, BackendRoutingEdge> wanted;
    QHash<QString, SendEndpoints> endpoints;
    // A partial declaration cannot remove only some of the user's sends.
    if (edges.size() > kMaxStrips * kMaxBuses) {
        for (const QString &key : m_active.keys()) retire(key, runtime, observed);
        m_refused = true;
        return;
    }
    for (const BackendRoutingEdge &edge : edges) {
        const QString key = routingNodeName(edge.stripId, edge.busId);
        if (wanted.contains(key) || !amplitude(edge).valid) {
            for (const QString &active : m_active.keys()) retire(active, runtime, observed);
            m_refused = true;
            return;
        }
        wanted.insert(key, edge);
        endpoints.insert(key, runtime.endpoints(edge));
    }
    for (const QString &key : m_active.keys()) {
        if (!wanted.contains(key) || endpoints.value(key) != m_active.value(key).endpoints
            || endpoints.value(key).source.isEmpty() || endpoints.value(key).target.isEmpty()) {
            retire(key, runtime, observed);
        }
    }

    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        const QString &key = it.key();
        const SendEndpoints endpoint = endpoints.value(key);
        if (endpoint.source.isEmpty() || endpoint.target.isEmpty()) continue;
        if (!m_active.contains(key)) {
            // AGENT-GUARD: deferred retirement is not destruction. Wait for
            // forget() before a replacement can carry this matrix cell, even
            // if a refused quiet write left the former module audible.
            if (m_retiring.values().contains(key)) continue;
            const auto declaration = m_admission.reserve(key);
            if (!declaration) { m_refused = true; continue; }
            const QByteArray arguments = ownedRoutingModuleArguments(*declaration,
                it->stripId, it->busId, endpoint.source, endpoint.target, endpoint.sourceIsSink);
            void *module = arguments.isEmpty() ? nullptr : runtime.load(key, arguments);
            if (!module) {
                m_admission.retire(declaration->instance);
                m_refused = true;
                continue;
            }
            m_active.insert(key, Send{module, *declaration, endpoint, {}});
        }
    }
    // Loading dispatches no event loop. Only a fresh actual observation after
    // the scoped Core slot has gone may release initial mute/zero state.
    const auto currentNodes = runtime.nodes();
    const auto currentProducer = runtime.producer();
    m_admission.observe(currentProducer, currentNodes);
    for (const QString &key : m_active.keys()) {
        Send &send = m_active[key];
        const auto playback = m_admission.playback(send.declaration.instance);
        if (!playback) {
            if (m_admission.invalidInstances().contains(send.declaration.instance)
                || currentProducer != producer) {
                retire(key, runtime, currentProducer == producer ? currentNodes
                                                               : QList<ObservedSendNode>{});
                m_refused = true;
            }
            continue;
        }
        send.lastPlayback = playback;
        if (!runtime.write(*playback, amplitude(wanted.value(key)))) {
            retire(key, runtime, currentNodes);
            m_refused = true;
        }
    }
}

void ConsoleSendReconciler::forget(void *module)
{
    m_retiring.remove(module);
    for (const QString &key : m_active.keys()) {
        if (m_active.value(key).module == module) {
            m_admission.retire(m_active.value(key).declaration.instance);
            m_active.remove(key);
        }
    }
}

void ConsoleSendReconciler::stop(const ConsoleSendRuntime &runtime)
{
    QList<void *> modules = m_retiring.keys();
    for (const Send &send : m_active) modules.append(send.module);
    m_active.clear();
    m_retiring.clear();
    m_admission.revoke();
    m_producer = {};
    for (void *module : modules) runtime.destroy(module);
    m_refused = false;
}
} // namespace QindaQt::Audio

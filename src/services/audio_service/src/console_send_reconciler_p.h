// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "console_routing_gain_p.h"
#include "console_send_admission_p.h"
#include <qindaqt/services/audio_service/audio_backend.h>

#include <functional>

namespace QindaQt::Audio
{
struct SendEndpoints {
    QString source;
    QString target;
    bool sourceIsSink = false;
    friend bool operator==(const SendEndpoints &, const SendEndpoints &) = default;
};

// All callbacks are synchronous, borrowed for one reconcile/stop call and run
// on the owning worker. Load may return only a dependency-admitted module;
// write must recheck the exact observed node and return actual setter success.
// Retirement schedules destruction outside PipeWire dispatch; its eventual
// callback calls forget() before the old address can carry a replacement.
struct ConsoleSendRuntime {
    std::function<SendEndpoints(const BackendRoutingEdge &)> endpoints;
    std::function<SendProducerIdentity()> producer;
    std::function<QList<ObservedSendNode>()> nodes;
    std::function<void *(const QString &, const QByteArray &)> load;
    std::function<bool(const ObservedSendNode &, ConsoleSendAmplitude)> write;
    std::function<void(void *)> retire;
    std::function<void(void *)> destroy;
};

// Private console route policy. It owns admission and module identity while
// the runtime owns actual PipeWire handles. No graph lookup by name grants a
// volume write. Configured gains remain public Console facts, not PCM proof.
class ConsoleSendReconciler final
{
public:
    void reconcile(const QList<BackendRoutingEdge> &edges, const ConsoleSendRuntime &runtime);
    void forget(void *module);
    void stop(const ConsoleSendRuntime &runtime);
    [[nodiscard]] const ConsoleSendAdmission &admission() const noexcept { return m_admission; }
    [[nodiscard]] bool refused() const noexcept { return m_refused; }
    [[nodiscard]] qsizetype size() const noexcept { return m_active.size(); }

private:
    struct Send {
        void *module = nullptr;
        SendModuleDeclaration declaration;
        SendEndpoints endpoints;
        std::optional<ObservedSendNode> lastPlayback;
    };
    void retire(const QString &key, const ConsoleSendRuntime &runtime,
                const QList<ObservedSendNode> &observed);
    void quiet(const Send &send, const ConsoleSendRuntime &runtime,
               const QList<ObservedSendNode> &observed);

    ConsoleSendAdmission m_admission;
    SendProducerIdentity m_producer;
    QHash<QString, Send> m_active;
    QHash<void *, QString> m_retiring;
    bool m_refused = false;
};
} // namespace QindaQt::Audio

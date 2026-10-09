// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "console_send_reconciler_p.h"
#include <QtCore/QRegularExpression>
#include <memory>
#include <vector>

namespace QindaQt::Tests
{
// Graph-free policy fixture. Observations/setter outcomes are injected, never
// represented as actual PipeWire identity, PCM or dependency qualification.
class ConsoleSendFixture final
{
public:
    struct Module { QString key; QByteArray arguments; };
    struct Write { Audio::ObservedSendNode node; Audio::ConsoleSendAmplitude amplitude; };
    Audio::SendProducerIdentity producer{1, 42, 420};
    QList<Audio::ObservedSendNode> nodes;
    std::vector<std::unique_ptr<Module>> modules;
    QList<void *> retired;
    QList<void *> destroyed;
    QList<Write> writes;
    bool announce = true;
    bool acceptWrite = true;
    bool acceptLoad = true;
    quint64 serial = 100;
    std::function<Audio::SendEndpoints(const Audio::BackendRoutingEdge &)> resolve =
        [](const Audio::BackendRoutingEdge &edge) {
            return Audio::SendEndpoints{QStringLiteral("source.%1").arg(edge.source.serial),
                QStringLiteral("output.%1").arg(edge.target.serial), edge.sourceIsSink};
        };

    Audio::ConsoleSendRuntime runtime()
    {
        return {
            .endpoints = [this](const auto &edge) { return resolve(edge); },
            .producer = [this] { return producer; },
            .nodes = [this] { return nodes; },
            .load = [this](const QString &key, const QByteArray &arguments) -> void * {
                if (!acceptLoad) return nullptr;
                modules.push_back(std::make_unique<Module>(Module{key, arguments}));
                if (announce) publishPair(*modules.back());
                return modules.back().get();
            },
            .write = [this](const auto &node, const auto amplitude) {
                writes.append({node, amplitude});
                return acceptWrite;
            },
            .retire = [this](void *module) { retired.append(module); },
            .destroy = [this](void *module) { destroyed.append(module); },
        };
    }

    void publishPair(const Module &module)
    {
        static const QRegularExpression token(
            QStringLiteral("qindaqt.console.send.instance = \"([0-9a-f]{32})\""));
        const QString value = token.match(QString::fromUtf8(module.arguments)).captured(1);
        for (const bool playback : {false, true}) {
            const quint64 id = ++serial;
            nodes.append({id, static_cast<quint32>(id), static_cast<quintptr>(id + 1000),
                producer.clientId, module.key + (playback ? QString{} : QStringLiteral(".capture")),
                value, playback ? QStringLiteral("playback") : QStringLiteral("capture"),
                playback ? QStringLiteral("Stream/Output/Audio") : QStringLiteral("Stream/Input/Audio")});
        }
    }

    void completeRetirement(Audio::ConsoleSendReconciler &controller, void *module)
    {
        const QString key = static_cast<Module *>(module)->key;
        nodes.removeIf([&key](const auto &node) {
            return node.name == key || node.name == key + QStringLiteral(".capture");
        });
        controller.forget(module);
    }
};
} // namespace QindaQt::Tests

// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include <utility>

namespace QindaQt::Audio
{

void WirePlumberWorker::applyVban(QList<BackendVbanStream> streams)
{
    invoke([this, streams = std::move(streams)] { applyVbanOnWorker(streams); });
}

void WirePlumberWorker::applyVbanOnWorker(const QList<BackendVbanStream> &streams)
{
    m_declaredVban = streams;
    if (m_core == nullptr || m_manager == nullptr) {
        return;
    }
    struct pw_context *const context = wp_core_get_pw_context(m_core);
    if (context == nullptr) {
        return;
    }
    // Stop what is no longer declared, or declared differently.
    for (auto it = m_vbanRuns.begin(); it != m_vbanRuns.end();) {
        const BackendVbanStream *wanted = nullptr;
        for (const BackendVbanStream &stream : streams) {
            if (stream.name.toStdString() == it->first) {
                wanted = &stream;
            }
        }
        if (wanted == nullptr || *wanted != it->second.declared) {
            it = m_vbanRuns.erase(it);   // destructors stop the threads and streams
        } else {
            ++it;
        }
    }
    for (const BackendVbanStream &stream : streams) {
        const std::string key = stream.name.toStdString();
        if (m_vbanRuns.find(key) != m_vbanRuns.end()) {
            continue;
        }
        VbanRun run;
        run.declared = stream;
        bool started = false;
        if (stream.outgoing) {
            const QString node = nodeNameForHandle(stream.target);
            if (node.isEmpty()) {
                continue;   // the device is not in the graph yet; next rebuild
            }
            run.sender = std::make_unique<VbanSender>();
            started = run.sender->start(context, node, stream.name, stream.host,
                                        static_cast<quint16>(stream.port));
        } else {
            run.receiver = std::make_unique<VbanReceiver>();
            started = run.receiver->start(context, stream.name, static_cast<quint16>(stream.port));
        }
        if (started) {
            m_vbanRuns.emplace(key, std::move(run));
        }
    }
}

void WirePlumberWorker::stopAllVban()
{
    m_vbanRuns.clear();
}

} // namespace QindaQt::Audio

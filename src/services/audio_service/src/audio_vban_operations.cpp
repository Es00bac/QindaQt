// SPDX-License-Identifier: GPL-3.0-or-later

// Manual peer stream projection and declarative graph publication (ADR-0246).
#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

namespace QindaQt::Audio
{

QList<VbanStream> AudioOperationCoordinator::vbanStreams() const
{
    QList<VbanStream> streams = m_vban.load();
    const QStringList enabled = m_console.enabledVbanStreams();
    for (VbanStream &stream : streams) {
        stream.enabled = enabled.contains(stream.name);
        bool declared = false;
        for (const BackendVbanStream &published : m_publishedVban)
            declared = declared || published.name == stream.name;
        // Worker evidence requires the local sender capture or authorized
        // receiver source+speaker route to appear in PipeWire. It does not
        // prove remote packet delivery or that a human hears audio.
        stream.active = stream.enabled && declared
            && m_runningVban.contains(stream.name);
    }
    return streams;
}

void AudioOperationCoordinator::publishVban()
{
    QList<BackendVbanStream> wanted;
    const QStringList enabled = m_console.enabledVbanStreams();
    const Console console = m_console.console();
    for (const VbanStream &stream : m_vban.load()) {
        if (!enabled.contains(stream.name)) {
            continue;
        }
        BackendVbanStream declared;
        declared.name = stream.name;
        declared.outgoing = stream.outgoing;
        declared.host = stream.host;
        declared.port = stream.port;
        if (stream.outgoing) {
            bool bound = false;
            for (const Bus &bus : console.buses) {
                if (bus.id == stream.busId && bus.targetKnown) {
                    declared.target = Handle{bus.targetEpoch, bus.targetSerial};
                    bound = true;
                }
            }
            if (!bound) continue;
        } else {
            bool bound = false;
            for (const Device &device : m_snapshot.outputs) {
                if (device.nodeName == stream.outputNodeName
                    && isPhysicalPeerOutput(device)) {
                    declared.target = device.handle;
                    bound = true;
                    break;
                }
            }
            // AGENT-GUARD: an absent authorized speaker leaves the receive
            // path down; it must never route to the default or virtual sink.
            if (!bound) continue;
        }
        wanted.append(declared);
    }
    if (wanted == m_publishedVban) {
        return;
    }
    m_publishedVban = wanted;
    if (m_backend != nullptr && m_running) {
        m_backend->applyVban(m_publishedVban);
    }
}

void AudioOperationCoordinator::acceptVbanRunning(const quint64 generation,
                                                   const QStringList &names)
{
    if (!m_running || generation != m_backendGeneration) return;
    if (m_runningVban == names) return;
    m_runningVban = names;
    republishConsole();
}

} // namespace QindaQt::Audio

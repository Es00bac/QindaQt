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
        bool connectedDeclaration = false;
        for (const BackendVbanStream &published : m_publishedVban)
            connectedDeclaration = connectedDeclaration
                || (published.name == stream.name && m_runningVban.contains(published));
        // Worker evidence requires the local sender capture or authorized
        // receiver source+speaker route to appear in PipeWire. It does not
        // prove remote packet delivery or that a human hears audio.
        stream.active = stream.enabled && connectedDeclaration;
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
        // Preserve the token only for the same continuously declared route.
        // Retargeting or re-enabling after removal gets a fresh identity, so
        // delayed worker evidence for an older route cannot become active.
        for (const BackendVbanStream &previous : m_publishedVban) {
            BackendVbanStream sameRoute = declared;
            sameRoute.activationToken = previous.activationToken;
            if (sameRoute == previous) {
                declared.activationToken = previous.activationToken;
                break;
            }
        }
        if (declared.activationToken == 0) {
            if (m_nextVbanActivationToken == 0) continue; // exhausted: fail closed
            declared.activationToken = m_nextVbanActivationToken++;
        }
        wanted.append(declared);
    }
    if (wanted == m_publishedVban) {
        return;
    }
    // Retire evidence for a replaced or removed declaration before this
    // snapshot can claim it active. Delayed old reports are also harmless:
    // vbanStreams compares the full declaration, not just its name.
    m_runningVban.removeIf([&wanted](const BackendVbanStream &running) {
        return !wanted.contains(running);
    });
    m_publishedVban = wanted;
    if (m_backend != nullptr && m_running) {
        m_backend->applyVban(m_publishedVban);
    }
}

void AudioOperationCoordinator::acceptVbanRunning(
    const quint64 generation, const QList<BackendVbanStream> &running)
{
    if (!m_running || generation != m_backendGeneration) return;
    if (m_runningVban == running) return;
    m_runningVban = running;
    republishConsole();
}

} // namespace QindaQt::Audio

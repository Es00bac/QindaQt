// SPDX-License-Identifier: LGPL-3.0-or-later

// The console half of the coordinator (ADR-0173), split out so
// audio_operation_coordinator.cpp stays inside its source-shape budget and so
// the console's own concerns - which operations it owns, how its routing
// reaches the backend, how it is republished - read together.

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

namespace QindaQt::Audio
{

bool AudioOperationCoordinator::isConsoleOperation(const OperationKind kind) noexcept
{
    switch (kind) {
    case OperationKind::SetStripGain:
    case OperationKind::SetStripMute:
    case OperationKind::SetStripSolo:
    case OperationKind::SetStripMono:
    case OperationKind::SetStripPan:
    case OperationKind::SetStripTrim:
    case OperationKind::SetStripSend:
    case OperationKind::SetBusGain:
    case OperationKind::SetBusMute:
    case OperationKind::SetBusMono:
    case OperationKind::SetBusTarget:
        return true;
    default:
        return false;
    }
}

void AudioOperationCoordinator::publishRouting()
{
    // AGENT-CONTRACT: the console names its endpoints by console id; the graph
    // needs the device handles behind them. Resolving here keeps the console
    // model free of graph concepts and keeps the backend free of console ones.
    QList<BackendRoutingEdge> edges;
    const Console console = m_console.console();
    for (const ConsoleModel::RoutingEdge &edge : m_console.routing()) {
        BackendRoutingEdge backendEdge;
        backendEdge.stripId = edge.stripId;
        backendEdge.busId = edge.busId;
        backendEdge.gainDb = edge.gainDb;
        backendEdge.audible = edge.audible;
        for (const Strip &strip : console.strips) {
            if (strip.id == edge.stripId && strip.sourceKnown) {
                backendEdge.source = Handle{strip.sourceEpoch, strip.sourceSerial};
                break;
            }
        }
        for (const Bus &bus : console.buses) {
            if (bus.id == edge.busId && bus.targetKnown) {
                backendEdge.target = Handle{bus.targetEpoch, bus.targetSerial};
                break;
            }
        }
        edges.append(std::move(backendEdge));
    }
    if (edges == m_publishedRouting) {
        return;
    }
    m_publishedRouting = edges;
    if (m_backend != nullptr && m_running) {
        m_backend->applyRouting(m_publishedRouting);
    }
}

void AudioOperationCoordinator::republishConsole()
{
    publishRouting();
    m_snapshot.console = m_console.console();
    // A console change is a real revision: clients diff on lineage, so a fader
    // move that left the revision alone would not reach any of them.
    if (m_snapshot.revision != std::numeric_limits<quint64>::max()) {
        ++m_snapshot.revision;
    }
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}


} // namespace QindaQt::Audio

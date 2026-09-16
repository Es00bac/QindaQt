// SPDX-License-Identifier: LGPL-3.0-or-later

// The console half of the coordinator (ADR-0173), split out so
// audio_operation_coordinator.cpp stays inside its source-shape budget and so
// the console's own concerns - which operations it owns, how its routing
// reaches the backend, how it is republished - read together.

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

#include "console_endpoints_p.h"

#include <QtCore/QSet>

#include <algorithm>

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
    case OperationKind::SetStripSource:
        return true;
    default:
        return false;
    }
}

namespace {

// The devices a console endpoint may follow, in the order it should claim them:
// the default first, because that is what the user means by "my microphone" or
// "my speakers", then the rest by serial so the assignment is stable across
// publications rather than following whatever order the graph enumerated.
[[nodiscard]] QList<Handle> bindableDevices(const QList<Device> &devices,
                                            const Handle &preferred,
                                            const QSet<QString> &claimed)
{
    QList<Device> candidates;
    for (const Device &device : devices) {
        // Neither a user-managed virtual device nor one of the console's own
        // endpoints is hardware the user plugged in; claiming either here would
        // have a hardware strip metering a bus's own sink. A device the user
        // pinned to a specific element is that element's alone.
        if (!device.virtualDevice
            && !ConsoleEndpoints::isConsoleOwnedNodeName(device.nodeName)
            && !claimed.contains(device.nodeName)) {
            candidates.append(device);
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Device &left, const Device &right) {
                  return left.handle.serial < right.handle.serial;
              });
    QList<Handle> order;
    order.reserve(candidates.size());
    for (const Device &device : candidates) {
        if (device.handle == preferred) {
            order.prepend(device.handle);
        } else {
            order.append(device.handle);
        }
    }
    return order;
}

} // namespace

namespace {

// The handle of the output device with this exact node.name, or an invalid
// handle when the graph does not have it (yet).
[[nodiscard]] Handle outputNamed(const Snapshot &snapshot, const QString &nodeName)
{
    for (const Device &device : snapshot.outputs) {
        if (device.nodeName == nodeName) {
            return device.handle;
        }
    }
    return {};
}

[[nodiscard]] Handle inputNamed(const Snapshot &snapshot, const QString &nodeName)
{
    for (const Device &device : snapshot.inputs) {
        if (device.nodeName == nodeName) {
            return device.handle;
        }
    }
    return {};
}

} // namespace

void AudioOperationCoordinator::autoBindConsole(const Snapshot &snapshot)
{
    const Console console = m_console.console();
    // Pinned devices are spoken for before anything automatic is assigned
    // (ADR-0178): the automatic rule never hands a hardware strip the
    // microphone the user pinned to another one.
    QSet<QString> claimed;
    for (const Strip &strip : console.strips) {
        if (!strip.pinnedSource.isEmpty()) {
            claimed.insert(strip.pinnedSource);
        }
    }
    for (const Bus &bus : console.buses) {
        if (!bus.pinnedTarget.isEmpty()) {
            claimed.insert(bus.pinnedTarget);
        }
    }
    const QList<Handle> inputs =
        bindableDevices(snapshot.inputs, snapshot.defaultInput, claimed);
    const QList<Handle> outputs =
        bindableDevices(snapshot.outputs, snapshot.defaultOutput, claimed);

    qsizetype nextInput = 0;
    for (const Strip &strip : console.strips) {
        if (strip.kind == StripKind::VirtualInput) {
            // A virtual strip IS its own sink (ADR-0175): it is bound the
            // moment the graph has the node of that name, and to nothing else.
            const Handle sink =
                outputNamed(snapshot, ConsoleEndpoints::stripSinkNodeName(strip.id));
            m_console.bindStripSource(strip.id, sink, sink.isValid());
            continue;
        }
        if (!strip.pinnedSource.isEmpty()) {
            // AGENT-GUARD: a pin to an absent device leaves the strip UNBOUND.
            // Falling back to automatic would quietly route another microphone
            // through the strip the user configured for this one.
            const Handle pinned = inputNamed(snapshot, strip.pinnedSource);
            m_console.bindStripSource(strip.id, pinned, pinned.isValid());
            continue;
        }
        // A strip beyond the number of real inputs is left UNBOUND rather than
        // sharing a device with another strip: two faders on one microphone
        // would look like a console that works and behave like one that does
        // not.
        const bool available = nextInput < inputs.size();
        m_console.bindStripSource(strip.id, available ? inputs.at(nextInput) : Handle{},
                                  available);
        if (available) {
            ++nextInput;
        }
    }
    qsizetype nextOutput = 0;
    for (const Bus &bus : console.buses) {
        if (bus.kind == BusKind::Virtual) {
            // Sends play into the bus's SINK; the source applications record
            // from is the other half of the same loopback and needs no binding.
            const Handle sink =
                outputNamed(snapshot, ConsoleEndpoints::busSinkNodeName(bus.id));
            m_console.bindBusTarget(bus.id, sink, sink.isValid());
            continue;
        }
        if (!bus.pinnedTarget.isEmpty()) {
            const Handle pinned = outputNamed(snapshot, bus.pinnedTarget);
            m_console.bindBusTarget(bus.id, pinned, pinned.isValid());
            continue;
        }
        const bool available = nextOutput < outputs.size();
        m_console.bindBusTarget(bus.id, available ? outputs.at(nextOutput) : Handle{},
                                available);
        if (available) {
            ++nextOutput;
        }
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
        backendEdge.pan = edge.pan;
        backendEdge.audible = edge.audible;
        for (const Strip &strip : console.strips) {
            if (strip.id == edge.stripId && strip.sourceKnown) {
                backendEdge.source = Handle{strip.sourceEpoch, strip.sourceSerial};
                backendEdge.sourceIsSink = strip.kind == StripKind::VirtualInput;
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

void AudioOperationCoordinator::publishMetering()
{
    // Every element whose device the graph currently has, whether or not the
    // user has routed it: a strip shows its input level before it is assigned
    // anywhere, which is how the user finds the right input in the first place.
    QList<BackendMeterTarget> targets;
    const Console console = m_console.console();
    for (const Strip &strip : console.strips) {
        if (strip.sourceKnown) {
            targets.append(BackendMeterTarget{
                .consoleId = strip.id,
                .device = Handle{strip.sourceEpoch, strip.sourceSerial},
                // A virtual strip IS a sink that applications play into, so its
                // audio is on its monitor, not on a capture port.
                .captureSink = strip.kind == StripKind::VirtualInput,
                // AGENT-GUARD: a strip meter never opens its device by itself.
                // It lights when the strip is actually carrying audio - the
                // send's loopback has the device open - or when another
                // application is already capturing. Metering every bound input
                // eagerly would hold every microphone and webcam on the machine
                // open for as long as the desktop is running.
                .passive = true});
        }
    }
    for (const Bus &bus : console.buses) {
        if (bus.targetKnown) {
            targets.append(
                BackendMeterTarget{.consoleId = bus.id,
                                   .device = Handle{bus.targetEpoch, bus.targetSerial},
                                   .captureSink = true,
                                   // A bus drives an output. Reading its
                                   // monitor turns nothing on and records
                                   // nothing the user is not already hearing,
                                   // so the mix meter is live immediately.
                                   .passive = false});
        }
    }
    if (targets == m_publishedMetering) {
        return;
    }
    m_publishedMetering = targets;
    if (m_backend != nullptr && m_running) {
        m_backend->applyMetering(m_publishedMetering);
    }
}

void AudioOperationCoordinator::publishConsoleEndpoints()
{
    QList<BackendConsoleEndpoint> endpoints;
    const Console console = m_console.console();
    for (const Strip &strip : console.strips) {
        if (strip.kind == StripKind::VirtualInput) {
            endpoints.append(BackendConsoleEndpoint{
                .consoleId = strip.id, .isBus = false, .description = strip.label});
        }
    }
    for (const Bus &bus : console.buses) {
        if (bus.kind == BusKind::Virtual) {
            endpoints.append(BackendConsoleEndpoint{
                .consoleId = bus.id, .isBus = true, .description = bus.label});
        }
    }
    if (endpoints == m_publishedEndpoints) {
        return;
    }
    m_publishedEndpoints = endpoints;
    if (m_backend != nullptr && m_running) {
        m_backend->applyConsoleEndpoints(m_publishedEndpoints);
    }
}

void AudioOperationCoordinator::acceptLevels(const quint64 generation,
                                             const QList<LevelReading> &levels)
{
    if (generation != m_backendGeneration || !m_running) {
        return;
    }
    // AGENT-GUARD: levels are folded into the retained console so a client that
    // connects mid-stream sees live meters in its first snapshot, but they do
    // NOT advance the revision - see LevelReading. Only the dedicated signal
    // carries them onward.
    if (!m_console.publishLevels(levels)) {
        return;
    }
    m_snapshot.console = m_console.console();
    Q_EMIT levelsChanged(levels);
}

void AudioOperationCoordinator::republishConsole()
{
    publishConsoleEndpoints();
    publishRouting();
    publishMetering();
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

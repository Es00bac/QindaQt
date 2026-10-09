// SPDX-License-Identifier: LGPL-3.0-or-later
// Default/pin/endpoint binding policy only, extracted without changing it.
// Default selection never rewrites pinned buses, explicit app targets or
// persisted presets (ADR0178); automatic buses may follow the current default.
#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include "console_endpoints_p.h"
#include <QtCore/QSet>
#include <algorithm>

namespace QindaQt::Audio
{
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

} // namespace QindaQt::Audio

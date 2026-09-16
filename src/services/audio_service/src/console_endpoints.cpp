// SPDX-License-Identifier: GPL-3.0-or-later

#include "console_endpoints_p.h"

#include "wireplumber_routing_p.h"

namespace QindaQt::Audio::ConsoleEndpoints
{

bool isConsoleOwnedNodeName(const QString &nodeName)
{
    return nodeName.startsWith(QLatin1String(kConsoleNodeNamePrefix));
}

QString stripSinkNodeName(const QString &stripId)
{
    return QLatin1String(kConsoleNodeNamePrefix) + stripId;
}

QString busSinkNodeName(const QString &busId)
{
    return QLatin1String(kConsoleNodeNamePrefix) + busId;
}

QString busSourceNodeName(const QString &busId)
{
    return busSinkNodeName(busId) + QStringLiteral(".source");
}

QList<QPair<QByteArray, QByteArray>> stripSinkProperties(const QString &stripId,
                                                         const QString &description)
{
    const QString sink = stripSinkNodeName(stripId);
    if (!routingNameIsEmbeddable(sink) || !routingNameIsEmbeddable(description)) {
        return {};
    }
    return {
        {"factory.name", "support.null-audio-sink"},
        {"node.name", sink.toUtf8()},
        {"node.description", description.toUtf8()},
        {"media.class", "Audio/Sink"},
        {"audio.position", "[ FL FR ]"},
        // The monitor follows the sink's own volume, so an application's level
        // into its strip is what the meter and the sends see.
        {"monitor.channel-volumes", "true"},
        // A device applications choose, never a stream looking for a target.
        {"node.autoconnect", "false"},
        // Outlives the proxy and the service: see the header.
        {"object.linger", "true"},
    };
}

QList<QPair<QByteArray, QByteArray>> busSinkProperties(const QString &busId,
                                                       const QString &description)
{
    const QString sink = busSinkNodeName(busId);
    if (!routingNameIsEmbeddable(sink) || !routingNameIsEmbeddable(description)) {
        return {};
    }
    return {
        {"factory.name", "support.null-audio-sink"},
        {"node.name", sink.toUtf8()},
        {"node.description", (description + QStringLiteral(" (bus)")).toUtf8()},
        {"media.class", "Audio/Sink"},
        {"audio.position", "[ FL FR ]"},
        {"monitor.channel-volumes", "true"},
        {"node.autoconnect", "false"},
        {"object.linger", "true"},
    };
}

QByteArray busModuleArguments(const QString &busId, const QString &description)
{
    const QString sink = busSinkNodeName(busId);
    const QString source = busSourceNodeName(busId);
    if (!routingNameIsEmbeddable(sink) || !routingNameIsEmbeddable(source)
        || !routingNameIsEmbeddable(description)) {
        return {};
    }
    // AGENT-GUARD: node.autoconnect = false on BOTH halves. module-loopback
    // makes streams that autoconnect by default; left on, WirePlumber treats
    // the bus's sink half as a stream wanting a target and links its monitor
    // into whatever sink it picks - the previous probe found two virtual buses
    // silently feeding a virtual strip. These are devices, not streams.
    return QStringLiteral(
               "{ node.description = \"%1\""
               " capture.props = { node.name = \"%2\" node.description = \"%1\""
               " media.class = Audio/Sink audio.position = [ FL FR ]"
               " node.autoconnect = false }"
               " playback.props = { node.name = \"%3\" node.description = \"%1\""
               " media.class = Audio/Source audio.position = [ FL FR ]"
               " node.autoconnect = false } }")
        .arg(description, sink, source)
        .toUtf8();
}

} // namespace QindaQt::Audio::ConsoleEndpoints

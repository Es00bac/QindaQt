// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wireplumber_routing_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

namespace QindaQt::Audio
{

QString routingNodeName(const QString &stripId, const QString &busId)
{
    // The managed prefix is what marks every node QindaQt may destroy; a send
    // loopback is as much QindaQt's to remove as a virtual device is.
    return QString::fromLatin1(kVirtualDeviceNamePrefix)
        + QStringLiteral("send.%1.%2").arg(stripId, busId);
}

bool routingNameIsEmbeddable(const QString &nodeName)
{
    if (nodeName.isEmpty() || nodeName.size() > kMaxDisplayNameUtf8Bytes) {
        return false;
    }
    for (const QChar character : nodeName) {
        const char16_t code = character.unicode();
        if (code == u'"' || code == u'\\' || code <= 0x001F || code == 0x007F) {
            return false;
        }
    }
    return true;
}

QByteArray routingModuleArguments(const QString &stripId, const QString &busId,
                                  const QString &sourceNodeName,
                                  const QString &targetNodeName,
                                  const double linear)
{
    if (!routingNameIsEmbeddable(sourceNodeName)
        || !routingNameIsEmbeddable(targetNodeName)) {
        return {};
    }
    const QString name = routingNodeName(stripId, busId);
    if (!routingNameIsEmbeddable(name)) {
        return {};
    }
    // node.passive keeps a send from holding the graph awake on its own: the
    // loopback follows whether real audio is flowing rather than pinning both
    // devices open for as long as the console has a cell switched on.
    const QString arguments =
        QStringLiteral(
            "{ node.name = \"%1\" node.description = \"QindaQt send %2 -> %3\""
            " capture.props = { node.target = \"%4\" stream.capture.sink = true"
            " node.passive = true }"
            " playback.props = { node.target = \"%5\" node.passive = true"
            " channelmix.normalize = false }"
            " target.object = \"%5\" }")
            .arg(name, stripId, busId, sourceNodeName, targetNodeName);
    Q_UNUSED(linear)
    return arguments.toUtf8();
}

} // namespace QindaQt::Audio

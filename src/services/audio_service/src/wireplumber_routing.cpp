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
                                  const bool sourceIsSink, const double linear)
{
    if (!routingNameIsEmbeddable(sourceNodeName)
        || !routingNameIsEmbeddable(targetNodeName)) {
        return {};
    }
    const QString name = routingNodeName(stripId, busId);
    if (!routingNameIsEmbeddable(name)) {
        return {};
    }
    // AGENT-GUARD: target.object, never the deprecated node.target. Only
    // target.object resolves a node NAME; node.target expects an object id and
    // silently autoconnects the default device instead, which puts the send on
    // the wrong endpoints while looking like it worked.
    //
    // AGENT-GUARD: neither side is passive. An enabled matrix cell is the user
    // saying "carry this audio"; a passive link will not resume a suspended
    // device, so a passive send is a routing the console draws and never plays.
    //
    // AGENT-CONTRACT: the PLAYBACK side carries the send's node name, because
    // that is the node whose volume is this matrix cell's gain. The worker
    // finds it by exactly this name to apply the fader, so the two must not
    // drift apart.
    const QString arguments =
        QStringLiteral(
            "{ node.name = \"%1\" node.description = \"QindaQt send %2 -> %3\""
            " capture.props = { node.name = \"%1.capture\" target.object = \"%4\""
            " stream.capture.sink = %6 }"
            " playback.props = { node.name = \"%1\" target.object = \"%5\""
            // Always stereo on the way out, whatever the source is: pan is two
            // channel volumes on this node (ADR-0177), and a mono microphone
            // would otherwise have nothing to pan on. The loopback's own
            // channel mixer does the upmix.
            " audio.position = [ FL FR ] channelmix.normalize = false }"
            " target.object = \"%5\" }")
            .arg(name, stripId, busId, sourceNodeName, targetNodeName,
                 // Only a strip that IS a sink - a virtual strip's null sink -
                 // is read from its monitor. Asking for a hardware capture
                 // device's monitor finds nothing and the send stays silent.
                 sourceIsSink ? QStringLiteral("true") : QStringLiteral("false"));
    Q_UNUSED(linear)
    return arguments.toUtf8();
}

} // namespace QindaQt::Audio

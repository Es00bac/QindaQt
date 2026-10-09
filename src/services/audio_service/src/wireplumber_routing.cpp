// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wireplumber_routing_p.h"
#include "console_routing_gain_p.h"
#include "console_send_admission_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

namespace QindaQt::Audio
{

QString routingNodeName(const QString &stripId, const QString &busId)
{
    // A deterministic address is not ownership. Send writes/destruction and
    // public projection require the actual producer/instance/serial admission
    // defined by ADR0361.
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

namespace {
QByteArray moduleArguments(const QString &stripId, const QString &busId,
                           const QString &sourceNodeName,
                           const QString &targetNodeName,
                           const bool sourceIsSink, const QString &captureProps,
                           const QString &playbackProps)
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
            // AGENT-GUARD: node.dont-fallback on both sides. Without it a send
            // whose target is momentarily absent is linked to the DEFAULT
            // device instead - a microphone strip quietly reading the webcam -
            // and never moved back. Unlinked until the target returns is the
            // only honest state.
            " capture.props = { node.name = \"%1.capture\" target.object = \"%4\""
            " stream.capture.sink = %6 node.dont-fallback = true%7 }"
            " playback.props = { node.name = \"%1\" target.object = \"%5\""
            " node.dont-fallback = true"
            // Always stereo on the way out, whatever the source is: pan is two
            // channel volumes on this node (ADR-0177), and a mono microphone
            // would otherwise have nothing to pan on. The loopback's own
            // channel mixer does the upmix.
            " audio.position = [ FL FR ] channelmix.normalize = false%8 }"
            " target.object = \"%5\" }")
            .arg(name, stripId, busId, sourceNodeName, targetNodeName,
                 // Only a strip that IS a sink - a virtual strip's null sink -
                 // is read from its monitor. Asking for a hardware capture
                 // device's monitor finds nothing and the send stays silent.
                 sourceIsSink ? QStringLiteral("true") : QStringLiteral("false"),
                 captureProps, playbackProps);
    return arguments.toUtf8();
}

} // namespace

QByteArray routingModuleArguments(const QString &stripId, const QString &busId,
                                  const QString &sourceNodeName,
                                  const QString &targetNodeName,
                                  const bool sourceIsSink, const double linear)
{
    Q_UNUSED(linear)
    return moduleArguments(stripId, busId, sourceNodeName, targetNodeName,
                           sourceIsSink, {}, {});
}

QByteArray ownedRoutingModuleArguments(const SendModuleDeclaration &declaration,
    const QString &stripId, const QString &busId, const QString &sourceNodeName,
    const QString &targetNodeName, const bool sourceIsSink)
{
    if (declaration.instance == 0 || declaration.token.size() != 32
        || declaration.playbackName != routingNodeName(stripId, busId)) {
        return {};
    }
    for (const QChar c : declaration.token) {
        if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
              || (c >= QLatin1Char('a') && c <= QLatin1Char('f')))) {
            return {};
        }
    }
    const QString capture = QStringLiteral(
        R"CFG( qindaqt.console.send.instance = "%1" qindaqt.console.send.role = capture)CFG")
        .arg(declaration.token);
    const QString playback = QStringLiteral(
        R"CFG( qindaqt.console.send.instance = "%1" qindaqt.console.send.role = playback)CFG"
        " state.restore-props = false channelmix.max-volume = %2"
        " node.param.Props = { mute = true channelVolumes = [ 0.0 0.0 ] }")
        .arg(declaration.token,
             QString::number(static_cast<double>(consoleSendMaximumAmplitude()), 'g', 17));
    return moduleArguments(stripId, busId, sourceNodeName, targetNodeName,
                           sourceIsSink, capture, playback);
}

} // namespace QindaQt::Audio

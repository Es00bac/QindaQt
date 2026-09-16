// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0173: each console send is realised as one libpipewire-module-loopback,
// because a plain port link cannot carry a gain and every matrix cell needs its
// own. The module argument is a string PipeWire parses internally: a malformed
// one fails with nothing to inspect, so its shape is pinned here.

#include "../../../src/services/audio_service/src/wireplumber_routing_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtTest>

using namespace QindaQt::Audio;

class WirePlumberRoutingTests final : public QObject
{
    Q_OBJECT
private slots:
    void sendNamesAreDeterministicAndManaged();
    void argumentsNameBothEndpointsAndTheSend();
    void hostileDeviceNamesCannotInjectProperties();
    void unusableNamesProduceNoArguments();
};

void WirePlumberRoutingTests::sendNamesAreDeterministicAndManaged()
{
    const QString name = routingNodeName(QStringLiteral("strip.hw.1"),
                                         QStringLiteral("bus.a2"));
    QCOMPARE(name, routingNodeName(QStringLiteral("strip.hw.1"),
                                   QStringLiteral("bus.a2")));
    // AGENT-GUARD: the managed prefix is what marks a node QindaQt may destroy.
    // A send loopback must carry it, or the service could not clean up its own
    // routing without risking a foreign node.
    QVERIFY(name.startsWith(QString::fromLatin1(kVirtualDeviceNamePrefix)));
    QVERIFY(name.contains(QStringLiteral("strip.hw.1")));
    QVERIFY(name.contains(QStringLiteral("bus.a2")));
    // Different cells never collide.
    QVERIFY(name != routingNodeName(QStringLiteral("strip.hw.1"),
                                    QStringLiteral("bus.a3")));
    QVERIFY(name != routingNodeName(QStringLiteral("strip.hw.2"),
                                    QStringLiteral("bus.a2")));
}

void WirePlumberRoutingTests::argumentsNameBothEndpointsAndTheSend()
{
    const QByteArray arguments = routingModuleArguments(
        QStringLiteral("strip.virtual.1"), QStringLiteral("bus.b1"),
        QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"),
        QStringLiteral("qindaqt.virtual.bus.b1"), true, 1.0);
    QVERIFY(!arguments.isEmpty());
    const QString text = QString::fromUtf8(arguments);
    // Capture from the strip's node, playback into the bus's node: a send that
    // named only one endpoint would silently attach to whatever PipeWire's
    // default happened to be.
    QVERIFY(text.contains(QStringLiteral("capture.props")));
    QVERIFY(text.contains(QStringLiteral("playback.props")));
    QVERIFY(text.contains(QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo")));
    QVERIFY(text.contains(QStringLiteral("qindaqt.virtual.bus.b1")));
    QVERIFY(text.contains(routingNodeName(QStringLiteral("strip.virtual.1"),
                                          QStringLiteral("bus.b1"))));
    // A VIRTUAL strip is a SINK the applications play into, so the capture side
    // has to take its monitor rather than treat it as a source.
    QVERIFY(text.contains(QStringLiteral("stream.capture.sink = true")));
    // AGENT-GUARD: target.object, not the deprecated node.target - only
    // target.object resolves a node name, and node.target silently attaches the
    // send to the default device instead.
    QVERIFY(!text.contains(QStringLiteral("node.target")));
    // AGENT-GUARD: an enabled cell must actually carry audio. A passive link
    // will not resume a suspended device, so a passive send is a routing the
    // console draws and never plays.
    QVERIFY(!text.contains(QStringLiteral("node.passive")));

    // A HARDWARE strip is a capture source, and asking for its monitor finds
    // nothing at all.
    const QString hardware = QString::fromUtf8(routingModuleArguments(
        QStringLiteral("strip.hw.1"), QStringLiteral("bus.a1"),
        QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"),
        QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"), false, 1.0));
    QVERIFY(hardware.contains(QStringLiteral("stream.capture.sink = false")));

    // AGENT-GUARD: the PLAYBACK node carries the send's own name, because that
    // is the node whose volume is this cell's gain and the worker finds it by
    // exactly this name. If the two ever drift apart the fader silently stops
    // reaching the audio while the console still shows it moving.
    const QString send = routingNodeName(QStringLiteral("strip.virtual.1"),
                                         QStringLiteral("bus.b1"));
    QVERIFY(text.contains(QStringLiteral("playback.props = { node.name = \"%1\"")
                              .arg(send)));
    // The capture side takes a distinct name so the two never collide in a
    // by-name lookup.
    QVERIFY(text.contains(QStringLiteral("%1.capture").arg(send)));
}

void WirePlumberRoutingTests::hostileDeviceNamesCannotInjectProperties()
{
    // A device name reaches PipeWire inside a quoted JSON-ish value. A crafted
    // one must not be able to close the quote and append its own properties.
    const QString crafted =
        QStringLiteral("evil\" } capture.props = { node.target = \"victim");
    QVERIFY(!routingNameIsEmbeddable(crafted));
    QVERIFY(routingModuleArguments(QStringLiteral("strip.hw.1"),
                                   QStringLiteral("bus.a1"), crafted,
                                   QStringLiteral("sink"), false, 1.0)
                .isEmpty());
    QVERIFY(routingModuleArguments(QStringLiteral("strip.hw.1"),
                                   QStringLiteral("bus.a1"),
                                   QStringLiteral("source"), crafted, false, 1.0)
                .isEmpty());
    QVERIFY(!routingNameIsEmbeddable(QStringLiteral("back\\slash")));
    QVERIFY(!routingNameIsEmbeddable(QStringLiteral("new\nline")));
}

void WirePlumberRoutingTests::unusableNamesProduceNoArguments()
{
    QVERIFY(!routingNameIsEmbeddable(QString()));
    QVERIFY(!routingNameIsEmbeddable(QString(kMaxDisplayNameUtf8Bytes + 1,
                                             QLatin1Char('x'))));
    QVERIFY(routingModuleArguments(QStringLiteral("strip.hw.1"),
                                   QStringLiteral("bus.a1"), QString(),
                                   QStringLiteral("sink"), false, 1.0)
                .isEmpty());
    // An ordinary name with dots, dashes and digits stays usable.
    QVERIFY(routingNameIsEmbeddable(
        QStringLiteral("alsa_output.usb-Generic_USB_Audio-00.analog-stereo")));
}

QTEST_APPLESS_MAIN(WirePlumberRoutingTests)
#include "tst_wireplumber_routing.moc"

// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0175: the graph nodes a console's virtual strips and buses are made of.
// Their names are the contract between the service, the worker and the shipped
// PipeWire drop-in, so the shape is pinned here.

#include "../../../src/services/audio_service/src/console_endpoints_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtTest>

using namespace QindaQt::Audio;

class ConsoleEndpointsTests final : public QObject
{
    Q_OBJECT
private slots:
    void namesAreDeterministicAndDistinctFromUserVirtualDevices();
    void aStripIsOneLingeringSink();
    void aBusIsASinkAndASourceWithTheSameFace();
    void hostileLabelsProduceNoArguments();
};

void ConsoleEndpointsTests::namesAreDeterministicAndDistinctFromUserVirtualDevices()
{
    const QString strip = ConsoleEndpoints::stripSinkNodeName(QStringLiteral("strip.virtual.1"));
    QCOMPARE(strip, ConsoleEndpoints::stripSinkNodeName(QStringLiteral("strip.virtual.1")));
    QVERIFY(ConsoleEndpoints::isConsoleOwnedNodeName(strip));
    // AGENT-GUARD: not the user-managed prefix. RemoveVirtualDevice may destroy
    // anything under that prefix, and the console's own sinks must not be
    // removable out from under the strips that are made of them.
    QVERIFY(!strip.startsWith(QString::fromLatin1(kVirtualDeviceNamePrefix)));
    QVERIFY(!ConsoleEndpoints::isConsoleOwnedNodeName(
        QString::fromLatin1(kVirtualDeviceNamePrefix) + QStringLiteral("game")));
    QVERIFY(!ConsoleEndpoints::isConsoleOwnedNodeName(
        QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo")));

    const QString bus = ConsoleEndpoints::busSinkNodeName(QStringLiteral("bus.b1"));
    const QString source = ConsoleEndpoints::busSourceNodeName(QStringLiteral("bus.b1"));
    QVERIFY(bus != source);
    QVERIFY(ConsoleEndpoints::isConsoleOwnedNodeName(source));
    QVERIFY(bus != ConsoleEndpoints::busSinkNodeName(QStringLiteral("bus.b2")));
    QVERIFY(strip != bus);
}

void ConsoleEndpointsTests::aStripIsOneLingeringSink()
{
    const auto properties = ConsoleEndpoints::stripSinkProperties(
        QStringLiteral("strip.virtual.2"), QStringLiteral("Virtual Input 2"));
    QVERIFY(!properties.isEmpty());
    QHash<QByteArray, QByteArray> map;
    for (const auto &[key, value] : properties) {
        map.insert(key, value);
    }
    QCOMPARE(map.value("factory.name"), QByteArray("support.null-audio-sink"));
    QCOMPARE(map.value("media.class"), QByteArray("Audio/Sink"));
    QCOMPARE(map.value("node.name"),
             ConsoleEndpoints::stripSinkNodeName(QStringLiteral("strip.virtual.2")).toUtf8());
    QCOMPARE(map.value("node.description"), QByteArray("Virtual Input 2"));
    // A device applications choose, never a stream looking for a target.
    QCOMPARE(map.value("node.autoconnect"), QByteArray("false"));
    // Linger: an application playing into its strip must keep that output
    // across an audio-service restart instead of landing on the speakers.
    QCOMPARE(map.value("object.linger"), QByteArray("true"));
    // The label reaches the daemon as a property value, but a label that could
    // not be embedded in a module argument is refused here too, so the two
    // endpoint kinds accept exactly the same names.
    QVERIFY(ConsoleEndpoints::stripSinkProperties(
                QStringLiteral("strip.virtual.2"),
                QStringLiteral("evil\" } capture.props = { target.object = \"victim"))
                .isEmpty());
}

void ConsoleEndpointsTests::aBusIsASinkAndASourceWithTheSameFace()
{
    const QString text = QString::fromUtf8(ConsoleEndpoints::busModuleArguments(
        QStringLiteral("bus.b1"), QStringLiteral("Virtual Bus B1")));
    QVERIFY(!text.isEmpty());
    // The capture half is the sink the sends play into; the playback half is a
    // real source applications record from. Both carry the user's label so the
    // bus looks like one device in every picker.
    QVERIFY(text.contains(QStringLiteral("capture.props = { node.name = \"%1\"")
                              .arg(ConsoleEndpoints::busSinkNodeName(QStringLiteral("bus.b1")))));
    QVERIFY(text.contains(QStringLiteral("playback.props = { node.name = \"%1\"")
                              .arg(ConsoleEndpoints::busSourceNodeName(QStringLiteral("bus.b1")))));
    QVERIFY(text.contains(QStringLiteral("media.class = Audio/Sink")));
    QVERIFY(text.contains(QStringLiteral("media.class = Audio/Source")));
    QCOMPARE(text.count(QStringLiteral("node.description = \"Virtual Bus B1\"")), 3);
    // AGENT-GUARD: both halves are devices. Left autoconnecting, WirePlumber
    // treats the sink half as a stream and links its monitor into whatever
    // sink it picks - two buses were found feeding a strip that way.
    QCOMPARE(text.count(QStringLiteral("node.autoconnect = false")), 2);
}

void ConsoleEndpointsTests::hostileLabelsProduceNoArguments()
{
    // The label reaches PipeWire inside a quoted argument: one that closes the
    // quote must not be able to append its own properties.
    QVERIFY(ConsoleEndpoints::busModuleArguments(
                QStringLiteral("bus.b1"),
                QStringLiteral("evil\" } capture.props = { target.object = \"victim"))
                .isEmpty());
    QVERIFY(ConsoleEndpoints::busModuleArguments(QStringLiteral("bus.b1"),
                                                 QStringLiteral("back\\slash"))
                .isEmpty());
}

QTEST_APPLESS_MAIN(ConsoleEndpointsTests)
#include "tst_console_endpoints.moc"

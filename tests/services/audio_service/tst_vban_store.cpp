// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../../src/services/audio_service/src/wireplumber_vban_p.h"

#include <qindaqt/services/audio_service/vban_store.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <arpa/inet.h>

using namespace QindaQt::Audio;

namespace {
VbanStream outgoingStream(QString host = QStringLiteral("192.0.2.20"))
{
    VbanStream stream;
    stream.name = QStringLiteral("Desk");
    stream.outgoing = true;
    stream.busId = QStringLiteral("bus.a1");
    stream.host = std::move(host);
    return stream;
}
VbanStream receiver(QString host = QStringLiteral("192.0.2.10"))
{
    VbanStream stream;
    stream.name = QStringLiteral("Laptop");
    stream.outgoing = false;
    stream.host = std::move(host);
    stream.outputNodeName = QStringLiteral("alsa_output.usb-Speakers.analog-stereo");
    return stream;
}
} // namespace

class VbanStoreTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void persistsTypedDefinitionsAtomically();
    void invalidIncomingConsentAndCorruptDocumentFailClosed();
    void noFallbackRouteNamesExactPhysicalOutput();
    void exactIpv4DatagramAdmission();
};

void VbanStoreTests::persistsTypedDefinitionsAtomically()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("nested/audio-vban.json"));
    VbanStore store(path);
    QString reason;
    QVERIFY(store.upsert(outgoingStream(), &reason));
    QVERIFY(store.upsert(receiver(), &reason));
    QCOMPARE(store.load().size(), 2);
    QVERIFY(QFile::exists(path));
    QCOMPARE(store.load().at(1).host, QStringLiteral("192.0.2.10"));
    QCOMPARE(store.load().at(1).outputNodeName,
             QStringLiteral("alsa_output.usb-Speakers.analog-stereo"));

    const QByteArray before = [&] {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray{};
        return file.readAll();
    }();
    QVERIFY(!before.isEmpty());
    VbanStream unsafe = receiver(QStringLiteral("0.0.0.0"));
    QVERIFY(!store.upsert(unsafe, &reason));
    QCOMPARE(reason, QStringLiteral("invalid-vban-stream"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), before);
    file.close();

    VbanStream conflicting = receiver(QStringLiteral("192.0.2.12"));
    conflicting.name = QStringLiteral("Other");
    QVERIFY(!store.upsert(conflicting, &reason));
    QCOMPARE(reason, QStringLiteral("vban-port-in-use"));
    QCOMPARE(store.load().size(), 2);
    VbanStream updated = receiver(QStringLiteral("192.0.2.11"));
    QVERIFY(store.upsert(updated, &reason));
    QCOMPARE(store.load().size(), 2);
    QCOMPARE(store.load().at(1).host, QStringLiteral("192.0.2.11"));
    QVERIFY(store.remove(QStringLiteral("Desk"), &reason));
    QCOMPARE(store.load().size(), 1);
    QCOMPARE(store.load().first().name, QStringLiteral("Laptop"));
    QVERIFY(!store.remove(QStringLiteral("Desk"), &reason));
    QCOMPARE(reason, QStringLiteral("unknown-vban-stream"));
}

void VbanStoreTests::invalidIncomingConsentAndCorruptDocumentFailClosed()
{
    QVERIFY(!validateVbanDefinition(receiver(QStringLiteral("peer.local"))).accepted);
    QVERIFY(!validateVbanDefinition(receiver(QStringLiteral("0.0.0.0"))).accepted);
    QVERIFY(!validateVbanDefinition(receiver(QStringLiteral("192.0.2.010"))).accepted);
    QVERIFY(validateVbanDefinition(receiver()).accepted);
    VbanStream noOutput = receiver();
    noOutput.outputNodeName.clear();
    QVERIFY(!validateVbanDefinition(noOutput).accepted);
    VbanStream injected = receiver();
    injected.outputNodeName = QStringLiteral("speaker\" } playback.props = {");
    QVERIFY(!validateVbanDefinition(injected).accepted);
    VbanStream unsafeName = outgoingStream();
    unsafeName.name = QStringLiteral("Desk\" } playback.props = {");
    QVERIFY(!validateVbanDefinition(unsafeName).accepted);
    unsafeName.name = QStringLiteral("Desk\\evil");
    QVERIFY(!validateVbanDefinition(unsafeName).accepted);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("audio-vban.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"incoming":[{"name":"Open","port":6980}],"outgoing":[]})");
    file.close();
    VbanStore store(path);
    QVERIFY(store.load().isEmpty()); // legacy wildcard receiver cannot run
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("{broken");
    file.close();
    QString reason;
    QVERIFY(!store.upsert(outgoingStream(), &reason));
    QCOMPARE(reason, QStringLiteral("invalid-vban-document"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{broken"));
}

void VbanStoreTests::noFallbackRouteNamesExactPhysicalOutput()
{
    const QString output = QStringLiteral("alsa_output.usb-Speakers.analog-stereo");
    const QString arguments = QString::fromUtf8(vbanRouteArguments(
        QStringLiteral("Laptop"), output));
    QVERIFY(arguments.contains(vbanSourceNodeName(QStringLiteral("Laptop"))));
    QVERIFY(arguments.contains(vbanRouteNodeName(QStringLiteral("Laptop"))));
    QVERIFY(arguments.contains(QStringLiteral("target.object = \"%1\"").arg(output)));
    QCOMPARE(arguments.count(QStringLiteral("node.dont-fallback = true")), 2);
    QVERIFY(vbanRouteArguments(QStringLiteral("Laptop"),
                               QStringLiteral("evil\" } playback.props = {")).isEmpty());
    QVERIFY(vbanRouteArguments(QStringLiteral("Laptop"), QString()).isEmpty());
}

void VbanStoreTests::exactIpv4DatagramAdmission()
{
    sockaddr_in expected{};
    sockaddr_in other{};
    expected.sin_family = AF_INET;
    other.sin_family = AF_INET;
    QVERIFY(::inet_pton(AF_INET, "127.0.0.1", &expected.sin_addr) == 1);
    QVERIFY(::inet_pton(AF_INET, "127.0.0.2", &other.sin_addr) == 1);
    QVERIFY(vbanSourceAllowed(expected, sizeof(expected), expected.sin_addr));
    QVERIFY(!vbanSourceAllowed(other, sizeof(other), expected.sin_addr));
    QVERIFY(!vbanSourceAllowed(expected, sizeof(expected) - 1, expected.sin_addr));
    expected.sin_family = AF_UNSPEC;
    QVERIFY(!vbanSourceAllowed(expected, sizeof(expected), expected.sin_addr));
}

QTEST_APPLESS_MAIN(VbanStoreTests)
#include "tst_vban_store.moc"

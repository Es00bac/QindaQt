// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_settings_test_support.h"
#include "src/apps/settings/audio/audio_peer_code.h"

#include <qindaqt/apps/settings_audio/audio_settings_model.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsAudio;
using namespace QindaQt::Apps::SettingsAudio::TestSupport;
using namespace QindaQt::Audio;

class AudioPeerCodeTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void connectionCodeRoundTripAndAdmission();
private:
  struct Fixture final {
    FakeAudioTransport transport;
    AudioClient client;
    AudioSettingsModel model;
    Fixture() : client(&transport), model(client) {
      client.setRequestTimeout(2'000);
      client.start();
      transport.announceOwner(QStringLiteral(":1.7"));
      QTRY_VERIFY_WITH_TIMEOUT(!transport.fetches.isEmpty(), 1'000);
      transport.reply(transport.fetches.constLast(), readyAudioSnapshot());
      QTRY_VERIFY_WITH_TIMEOUT(model.ready(), 1'000);
    }
  };
};

void AudioPeerCodeTest::connectionCodeRoundTripAndAdmission() {
  const PeerCode tuple{QStringLiteral("Desk"), QStringLiteral("192.0.2.10"), 6980};
  const QString code = encodePeerCode(tuple);
  QVERIFY(code.startsWith(QStringLiteral("QINDAQT-AUDIO-1:")));
  PeerCode decoded;
  QString reason;
  QVERIFY(decodePeerCode(code, &decoded, &reason));
  QCOMPARE(decoded.name, tuple.name);
  QCOMPARE(decoded.sourceIpv4, tuple.sourceIpv4);
  QCOMPARE(decoded.port, tuple.port);
  QVERIFY(!decodePeerCode(code + QStringLiteral("!"), &decoded, &reason));
  QCOMPARE(reason, QStringLiteral("invalid-code"));
  QVERIFY(!decodePeerCode(QStringLiteral("QINDAQT-AUDIO-2:AAAA"), &decoded, &reason));
  QCOMPARE(reason, QStringLiteral("foreign-code"));
  const QByteArray foreign = R"({"version":2,"name":"Desk","sourceIpv4":"192.0.2.10","port":6980})";
  const QString future = QStringLiteral("QINDAQT-AUDIO-1:")
      + QString::fromLatin1(foreign.toBase64(QByteArray::Base64UrlEncoding
                                            | QByteArray::OmitTrailingEquals));
  QVERIFY(!decodePeerCode(future, &decoded, &reason));
  QCOMPARE(reason, QStringLiteral("foreign-code"));
  const auto rawCode = [](const QByteArray &json) {
    return QStringLiteral("QINDAQT-AUDIO-1:")
        + QString::fromLatin1(json.toBase64(QByteArray::Base64UrlEncoding
                                            | QByteArray::OmitTrailingEquals));
  };
  QVERIFY(!decodePeerCode(rawCode(R"({"version":1,"name":"Desk","sourceIpv4":"192.0.2.10"})"),
                          &decoded, &reason));
  QCOMPARE(reason, QStringLiteral("invalid-code"));
  QVERIFY(!decodePeerCode(rawCode(R"({ "version": 1, "name":"Desk","sourceIpv4":"192.0.2.10","port":6980})"),
                          &decoded, &reason));
  QCOMPARE(reason, QStringLiteral("invalid-code"));
  QVERIFY(!decodePeerCode(rawCode(R"({"version":1,"name":"Desk","sourceIpv4":"192.0.2.10","port":"6980"})"),
                          &decoded, &reason));
  QCOMPARE(reason, QStringLiteral("invalid-code"));
  QVERIFY(encodePeerCode({QStringLiteral("Desk"), QStringLiteral("192.0.2.010"), 6980}).isEmpty());
  QVERIFY(encodePeerCode({QStringLiteral("Desk"), QStringLiteral("0.0.0.0"), 6980}).isEmpty());

  Fixture fixture;
  Snapshot snapshot = readyAudioSnapshot(11, 3);
  snapshot.capabilities |= Capability::Console | Capability::ManageVbanStreams;
  snapshot.outputs[0].nodeName = QStringLiteral("alsa_output.desk");
  Bus bus;
  bus.id = QStringLiteral("bus.a1");
  bus.label = QStringLiteral("Bus A1");
  snapshot.console.buses.append(bus);
  Strip strip;
  strip.id = QStringLiteral("strip.hw.1");
  strip.label = QStringLiteral("Mic");
  snapshot.console.strips.append(strip);
  VbanStream sender;
  sender.name = QStringLiteral("Stream1");
  sender.outgoing = true;
  sender.busId = bus.id;
  sender.host = QStringLiteral("192.0.2.20");
  sender.port = 6981;
  snapshot.console.vban.append(sender);
  QVERIFY(validateSnapshot(snapshot).accepted);
  fixture.transport.invalidate(QStringLiteral(":1.7"), 11, 3);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.transport.fetches.size() >= 2, 1000);
  fixture.transport.reply(fixture.transport.fetches.constLast(), snapshot);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.canManagePeerStreams(), 1000);
  const QVariantMap shared = fixture.model.sharePeerCode(sender.name,
                                                        QStringLiteral("192.0.2.10"));
  QVERIFY(shared.value(QStringLiteral("valid")).toBool());
  PeerCode sharedTuple;
  QVERIFY(decodePeerCode(shared.value(QStringLiteral("code")).toString(),
                         &sharedTuple, &reason));
  QCOMPARE(sharedTuple.name, sender.name);
  QCOMPARE(sharedTuple.sourceIpv4, QStringLiteral("192.0.2.10"));
  QCOMPARE(sharedTuple.port, sender.port);
  QVERIFY(!fixture.model.sharePeerCode(sender.name,
                                     QStringLiteral("192.0.2.010"))
               .value(QStringLiteral("valid")).toBool());
  QCOMPARE(fixture.model.reviewPeerCode(shared.value(QStringLiteral("code")).toString())
               .value(QStringLiteral("reason")).toString(), QStringLiteral("name-conflict"));

  QVERIFY(fixture.model.reviewPeerCode(code).value(QStringLiteral("valid")).toBool());
  QVERIFY(!fixture.model.saveImportedPeer(code, QStringLiteral("missing-output")));
  QCOMPARE(fixture.transport.operations.size(), 0);
  QVERIFY(fixture.model.saveImportedPeer(code, QStringLiteral("alsa_output.desk")));
  QCOMPARE(fixture.transport.operations.size(), 1);
  const auto operation = fixture.transport.operations.constLast();
  QCOMPARE(operation.request.kind, OperationKind::UpsertVbanStream);
  QCOMPARE(operation.request.vbanDefinition.name, tuple.name);
  QCOMPARE(operation.request.vbanDefinition.host, tuple.sourceIpv4);
  QCOMPARE(operation.request.vbanDefinition.outputNodeName,
           QStringLiteral("alsa_output.desk"));
  QVERIFY(!operation.request.vbanDefinition.enabled);
  fixture.transport.finish(operation, audioResult(OperationKind::UpsertVbanStream,
                                                   OperationStatus::Succeeded, 11, 3));
  QTRY_VERIFY_WITH_TIMEOUT(fixture.transport.fetches.size() >= 3, 1000);
  snapshot.revision = 4;
  snapshot.console.vban.append(operation.request.vbanDefinition);
  fixture.transport.reply(fixture.transport.fetches.constLast(), snapshot);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(4), 1000);
  QCOMPARE(fixture.model.reviewPeerCode(code).value(QStringLiteral("reason")).toString(),
           QStringLiteral("duplicate-peer"));
  const QString otherPort = encodePeerCode({QStringLiteral("Other"),
                                           tuple.sourceIpv4, tuple.port});
  QCOMPARE(fixture.model.reviewPeerCode(otherPort)
               .value(QStringLiteral("reason")).toString(), QStringLiteral("port-conflict"));
  const QString otherHost = encodePeerCode({tuple.name,
                                           QStringLiteral("192.0.2.11"), 6982});
  QCOMPARE(fixture.model.reviewPeerCode(otherHost)
               .value(QStringLiteral("reason")).toString(), QStringLiteral("name-conflict"));
  const QString fresh = encodePeerCode({QStringLiteral("Fresh"),
                                        QStringLiteral("192.0.2.30"), 6990});
  QVERIFY(fixture.model.reviewPeerCode(fresh).value(QStringLiteral("valid")).toBool());
  snapshot.revision = 5;
  snapshot.outputs[0].nodeName = QStringLiteral("alsa_output.replaced");
  QVERIFY(validateSnapshot(snapshot).accepted);
  fixture.transport.invalidate(QStringLiteral(":1.7"), 11, 5);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.transport.fetches.size() >= 4, 1000);
  fixture.transport.reply(fixture.transport.fetches.constLast(), snapshot);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(5), 1000);
  QVERIFY(!fixture.model.saveImportedPeer(fresh, QStringLiteral("alsa_output.desk")));
  QCOMPARE(fixture.transport.operations.size(), 1);

}

QTEST_MAIN(AudioPeerCodeTest)
#include "tst_audio_peer_code.moc"

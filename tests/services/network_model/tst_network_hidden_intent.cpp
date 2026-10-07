// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_protocol_test_data.h"
#include <QtTest>
#include <qindaqt/services/network_model/network_model.h>
#include <qindaqt/services/network_protocol/network_identity.h>
using namespace QindaQt::Network;
using namespace QindaQt::Network::Model;
using namespace QindaQt::Network::TestData;

class HiddenIntentTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void exactNamesAndBothChoices();
  void hostileNames_data();
  void hostileNames();
  void currentDeviceRadioAndDuplicate();
};
void HiddenIntentTests::exactNamesAndBothChoices() {
  const Snapshot snapshot = validSnapshot();
  for (const auto security :
       {SecuritySuite::Wpa2Personal, SecuritySuite::Wpa3Personal}) {
    for (const QString &ssid :
         {QStringLiteral("  literal spaces  "), QString(32, u'x'),
          QString(16, QChar(0x00e9)), QStringLiteral("<b>plain</b>")}) {
      const auto verdict = validateConnectHidden(
          snapshot, {QStringLiteral("wlan0"), ssid, security});
      QVERIFY2(verdict.allowed, qPrintable(verdict.reasonCode));
      QCOMPARE(verdict.kind, OperationKind::ConnectKnownNetwork);
    }
  }
}
void HiddenIntentTests::hostileNames_data() {
  QTest::addColumn<QString>("ssid");
  QTest::newRow("empty") << QString();
  QTest::newRow("ascii33") << QString(33, u'x');
  QTest::newRow("utf8overflow") << QString(17, QChar(0x00e9));
  QTest::newRow("newline") << QStringLiteral("a\nb");
  QTest::newRow("nul") << QString(QChar(0));
  QTest::newRow("surrogate") << QString(QChar(0xd800));
  QTest::newRow("bidi") << QString(QChar(0x202e));
}
void HiddenIntentTests::hostileNames() {
  QFETCH(QString, ssid);
  const auto verdict =
      validateConnectHidden(validSnapshot(), {QStringLiteral("wlan0"), ssid,
                                              SecuritySuite::Wpa2Personal});
  QVERIFY(!verdict.allowed);
  QCOMPARE(verdict.reasonCode, QStringLiteral("hidden-network-ssid-invalid"));
}
void HiddenIntentTests::currentDeviceRadioAndDuplicate() {
  Snapshot snapshot = validSnapshot();
  const ConnectHiddenIntent intent{QStringLiteral("wlan0"),
                                   QStringLiteral("New hidden"),
                                   SecuritySuite::Wpa3Personal};
  QVERIFY(validateConnectHidden(snapshot, intent).allowed);
  auto wrong = intent;
  wrong.deviceInterface = QStringLiteral("wlan9");
  QVERIFY(!validateConnectHidden(snapshot, wrong).allowed);
  wrong.deviceInterface = QStringLiteral(" wlan0");
  QVERIFY(!validateConnectHidden(snapshot, wrong).allowed);
  for (const auto security :
       {SecuritySuite::Open, SecuritySuite::Wep, SecuritySuite::Wpa2Enterprise,
        SecuritySuite(99)}) {
    wrong = intent;
    wrong.security = security;
    QCOMPARE(validateConnectHidden(snapshot, wrong).reasonCode,
             QStringLiteral("hidden-network-security-unsupported"));
  }
  snapshot.radios[0].softwareEnabled = false;
  QVERIFY(!validateConnectHidden(snapshot, intent).allowed);
  snapshot.radios[0].softwareEnabled = true;
  snapshot.radios[0].hardwareEnabled = false;
  QVERIFY(!validateConnectHidden(snapshot, intent).allowed);
  snapshot.radios[0].hardwareEnabled = true;
  snapshot.knownNetworks.append(
      {knownNetworkId(intent.ssid.toUtf8(), intent.security), intent.ssid,
       false, intent.security, true});
  QCOMPARE(validateConnectHidden(snapshot, intent).reasonCode,
           QStringLiteral("network-already-known"));
  snapshot.availability = Availability::Unavailable;
  QVERIFY(!validateConnectHidden(snapshot, intent).allowed);
}
QTEST_GUILESS_MAIN(HiddenIntentTests)
#include "tst_network_hidden_intent.moc"

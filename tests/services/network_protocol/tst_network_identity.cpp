// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtTest>

#include <algorithm>

using namespace QindaQt::Network;

class NetworkIdentityTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void normalizesPrintableUtf8Ssid();
  void mapsUnprintableOrBinarySsidsToHidden();
  void mapsUnicodeSpoofControlsAndUnassignedScalarsToHidden();
  void acceptsPrintableSupplementaryUnicode();
  void rejectsOversizedSsid();
  void normalizesBssid();
  void normalizesInterfaceName();
  void derivesStablePseudonymousNetworkId();
  void differentSecurityYieldsDifferentNetworkId();
};

void NetworkIdentityTests::normalizesPrintableUtf8Ssid() {
  const SsidIdentity identity = normalizeSsid(QByteArray("Café 5G"));
  QVERIFY(identity.valid);
  QVERIFY(!identity.hidden);
  QCOMPARE(identity.text, QStringLiteral("Café 5G"));
}

void NetworkIdentityTests::mapsUnicodeSpoofControlsAndUnassignedScalarsToHidden() {
  const SsidIdentity bidi =
      normalizeSsid(QByteArray::fromHex("e280ae") + QByteArray("spoof"));
  QVERIFY(bidi.valid);
  QVERIFY(bidi.hidden);
  QVERIFY(bidi.text.isEmpty());

  const SsidIdentity unassigned = normalizeSsid(QString(QChar(0x0378)).toUtf8());
  QVERIFY(unassigned.valid);
  QVERIFY(unassigned.hidden);

  QVERIFY(!isPresentationSafeText(QStringLiteral("Cafe\u202e")));
}

void NetworkIdentityTests::acceptsPrintableSupplementaryUnicode() {
  const QString text = QStringLiteral("Cafe \U0001f680");
  const SsidIdentity identity = normalizeSsid(text.toUtf8());
  QVERIFY(identity.valid);
  QVERIFY(!identity.hidden);
  QCOMPARE(identity.text, text);
  QVERIFY(isPresentationSafeText(text));
}

void NetworkIdentityTests::mapsUnprintableOrBinarySsidsToHidden() {
  const SsidIdentity controlChars = normalizeSsid(QByteArray("Bad\x01Ssid", 8));
  QVERIFY(controlChars.valid);
  QVERIFY(controlChars.hidden);
  QVERIFY(controlChars.text.isEmpty());

  const SsidIdentity invalidUtf8 = normalizeSsid(QByteArray("\xff\xfe", 2));
  QVERIFY(invalidUtf8.valid);
  QVERIFY(invalidUtf8.hidden);
  QVERIFY(invalidUtf8.text.isEmpty());

  const SsidIdentity empty = normalizeSsid(QByteArray());
  QVERIFY(empty.valid);
  QVERIFY(empty.hidden);

  const SsidIdentity embeddedNull = normalizeSsid(QByteArray("A\0B", 3));
  QVERIFY(embeddedNull.valid);
  QVERIFY(embeddedNull.hidden);
}

void NetworkIdentityTests::rejectsOversizedSsid() {
  QByteArray oversized(kMaxSsidRawBytes + 1, 'x');
  const SsidIdentity identity = normalizeSsid(oversized);
  QVERIFY(!identity.valid);
  QVERIFY(!identity.hidden);
  QVERIFY(identity.text.isEmpty());

  const SsidIdentity boundary = normalizeSsid(QByteArray(kMaxSsidRawBytes, 'x'));
  QVERIFY(boundary.valid);
  QVERIFY(!boundary.hidden);
}

void NetworkIdentityTests::normalizesBssid() {
  QString normalized;
  QVERIFY(normalizeBssid(QStringLiteral("AA:2B:3C:4D:5E:6F"), &normalized));
  QCOMPARE(normalized, QStringLiteral("aa:2b:3c:4d:5e:6f"));
  QVERIFY(!normalizeBssid(QStringLiteral("aa:2b:3c:4d:5e"), &normalized));
  QVERIFY(!normalizeBssid(QStringLiteral("zz:2b:3c:4d:5e:6f"), &normalized));
  QVERIFY(!normalizeBssid(QStringLiteral("aa-2b-3c-4d-5e-6f"), &normalized));
  QVERIFY(!normalizeBssid(QString(), &normalized));
}

void NetworkIdentityTests::normalizesInterfaceName() {
  QString normalized;
  QVERIFY(normalizeInterfaceName(QStringLiteral("wlan0"), &normalized));
  QCOMPARE(normalized, QStringLiteral("wlan0"));
  // A VLAN alias such as enp3s0.100 is a real interface name; the `@if`
  // suffix is rtnetlink display formatting, not part of the name.
  QVERIFY(normalizeInterfaceName(QStringLiteral("enp3s0.100"), &normalized));
  QCOMPARE(normalized, QStringLiteral("enp3s0.100"));
  QVERIFY(!normalizeInterfaceName(QStringLiteral("enp3s0.100@eth"),
                                  &normalized));
  QVERIFY(normalizeInterfaceName(QStringLiteral("w"), &normalized));
  QVERIFY(!normalizeInterfaceName(QStringLiteral(" wlan0"), &normalized));
  QVERIFY(!normalizeInterfaceName(QStringLiteral("-wlan"), &normalized));
  QVERIFY(!normalizeInterfaceName(QStringLiteral("wlan#0"), &normalized));
  QVERIFY(!normalizeInterfaceName(QString(16, 'a'), &normalized));
  QVERIFY(!normalizeInterfaceName(QString(), &normalized));
}

void NetworkIdentityTests::derivesStablePseudonymousNetworkId() {
  const QByteArray ssid("Cafe");
  const QString id = knownNetworkId(ssid, SecuritySuite::Wpa2Personal);
  QCOMPARE(id.size(), 64);
  QVERIFY(std::all_of(id.cbegin(), id.cend(), [](const QChar character) {
    const char16_t code = character.unicode();
    return (code >= u'0' && code <= u'9') || (code >= u'a' && code <= u'f');
  }));
  QCOMPARE(knownNetworkId(ssid, SecuritySuite::Wpa2Personal), id);
  // The digest must not embed the SSID or any decodable secret material.
  QVERIFY(!id.contains(QStringLiteral("Cafe"), Qt::CaseInsensitive));
}

void NetworkIdentityTests::differentSecurityYieldsDifferentNetworkId() {
  const QByteArray ssid("Cafe");
  QVERIFY(knownNetworkId(ssid, SecuritySuite::Wpa2Personal)
          != knownNetworkId(ssid, SecuritySuite::Open));
  QVERIFY(knownNetworkId(ssid, SecuritySuite::Wpa2Personal)
          != knownNetworkId(QByteArray("Other"), SecuritySuite::Wpa2Personal));
  QVERIFY(knownNetworkId(QByteArray(33, 'x'), SecuritySuite::Open).isEmpty());
}

QTEST_MAIN(NetworkIdentityTests)
#include "tst_network_identity.moc"

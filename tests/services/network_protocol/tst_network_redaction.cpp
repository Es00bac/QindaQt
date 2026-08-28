// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_redaction.h>

#include <QtTest>

using namespace QindaQt::Network;

class NetworkRedactionTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void recognizesSecretKeyNames();
  void detectsNestedSecrets();
  void rejectsOverdeepNesting();
  void ignoresNonSecretKeys();
  void redactsSecretAssignmentsInDiagnostics();
  void boundsAndCleansDiagnosticText();
};

void NetworkRedactionTests::recognizesSecretKeyNames() {
  QVERIFY(isSecretKeyName(QStringLiteral("password")));
  QVERIFY(isSecretKeyName(QStringLiteral("PassPhrase")));
  QVERIFY(isSecretKeyName(QStringLiteral("psk")));
  QVERIFY(isSecretKeyName(QStringLiteral("wpa-psk")));
  QVERIFY(isSecretKeyName(QStringLiteral("8021x-password")));
  QVERIFY(isSecretKeyName(QStringLiteral("agent-secret")));
  QVERIFY(!isSecretKeyName(QStringLiteral("ssid")));
  QVERIFY(!isSecretKeyName(QStringLiteral("password-hint-policy")));
  QVERIFY(!isSecretKeyName(QString()));
}

void NetworkRedactionTests::detectsNestedSecrets() {
  QVariantMap inner;
  inner.insert(QStringLiteral("psk"), QStringLiteral("hunter2"));
  QVariantMap outer;
  outer.insert(QStringLiteral("security"), inner);
  QVERIFY(wireContainsSecrets(outer));

  QVariantMap listCarrier;
  listCarrier.insert(QStringLiteral("profiles"),
                     QVariantList{QVariant(QVariantMap{{QStringLiteral("name"),
                                                        QStringLiteral("home")}}),
                                  QVariant(QVariantMap{{QStringLiteral("password"),
                                                        QStringLiteral("x")}})});
  QVERIFY(wireContainsSecrets(listCarrier));

  QVariantMap clean;
  clean.insert(QStringLiteral("ssid"), QStringLiteral("Cafe"));
  clean.insert(QStringLiteral("security"),
               QVariantMap{{QStringLiteral("suite"), QStringLiteral("wpa2")}});
  QVERIFY(!wireContainsSecrets(clean));
}

void NetworkRedactionTests::rejectsOverdeepNesting() {
  QVariant value = QVariantMap{{QStringLiteral("k"), QStringLiteral("v")}};
  for (int depth = 0; depth < 16; ++depth) {
    value = QVariantMap{{QStringLiteral("nested"), value}};
  }
  QVERIFY(wireContainsSecrets(value.toMap()));
}

void NetworkRedactionTests::ignoresNonSecretKeys() {
  QVariantMap intent;
  intent.insert(QStringLiteral("knownNetworkId"),
                QStringLiteral("0123456789abcdef"));
  intent.insert(QStringLiteral("deviceInterface"), QStringLiteral("wlan0"));
  intent.insert(QStringLiteral("deadlineMs"), 30'000);
  QVERIFY(!wireContainsSecrets(intent));
}

void NetworkRedactionTests::redactsSecretAssignmentsInDiagnostics() {
  const QString redacted = redactDiagnostic(
      QStringLiteral("connect failed: password=hunter2 psk: abc123; ssid=Home"));
  QVERIFY(redacted.contains(QStringLiteral("password=<redacted>")));
  QVERIFY(redacted.contains(QStringLiteral("psk=<redacted>")));
  QVERIFY(redacted.contains(QStringLiteral("ssid=Home")));
  QVERIFY(!redacted.contains(QStringLiteral("hunter2")));
  QVERIFY(!redacted.contains(QStringLiteral("abc123")));
}

void NetworkRedactionTests::boundsAndCleansDiagnosticText() {
  const QString noisy = QStringLiteral("a\01b\02c password=leak");
  const QString cleaned = redactDiagnostic(noisy);
  QVERIFY(!cleaned.contains(QChar(0x01)));
  QVERIFY(!cleaned.contains(QChar(0x02)));
  QVERIFY(cleaned.contains(QStringLiteral("password=<redacted>")));

  const QString huge(kMaxDiagnosticUtf8Bytes * 2, u'x');
  const QString bounded = redactDiagnostic(huge);
  QVERIFY(bounded.toUtf8().size() <= kMaxDiagnosticUtf8Bytes + 4);
}

QTEST_MAIN(NetworkRedactionTests)
#include "tst_network_redaction.moc"

// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_redaction.h>

#include <QtTest>

using namespace QindaQt::Network;

class NetworkRedactionTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void recognizesSecretKeyNames();
  void detectsNestedSecrets();
  void rejectsOverdeepNesting();
  void rejectsOverwideNesting();
  void rejectsOverbudgetNodeCount();
  void ignoresNonSecretKeys();
  void redactsSecretAssignmentsInDiagnostics();
  void redactsQuotedAndMalformedCredentialAssignments();
  void boundsAndCleansDiagnosticText();
};

void NetworkRedactionTests::recognizesSecretKeyNames() {
  QVERIFY(isSecretKeyName(QStringLiteral("password")));
  QVERIFY(isSecretKeyName(QStringLiteral("PassPhrase")));
  QVERIFY(isSecretKeyName(QStringLiteral("psk")));
  QVERIFY(isSecretKeyName(QStringLiteral("wpa-psk")));
  QVERIFY(isSecretKeyName(QStringLiteral("8021x-password")));
  QVERIFY(isSecretKeyName(QStringLiteral("agent-secret")));
  QVERIFY(isSecretKeyName(QStringLiteral("wifi.password")));
  QVERIFY(isSecretKeyName(QStringLiteral("wifi_psk")));
  QVERIFY(!isSecretKeyName(QStringLiteral("ssid")));
  QVERIFY(!isSecretKeyName(QStringLiteral("password-hint-policy")));
  QVERIFY(!isSecretKeyName(QString()));
}

void NetworkRedactionTests::rejectsOverwideNesting() {
  QVariantList hostile;
  for (qsizetype index = 0; index <= kMaximumIntentWireContainerEntries;
       ++index) {
    hostile.append(index);
  }
  const QVariantMap wire{{QStringLiteral("values"), hostile}};
  QVERIFY(wireContainsSecrets(wire));
}

void NetworkRedactionTests::rejectsOverbudgetNodeCount() {
  QVariantList leaves;
  for (qsizetype index = 0; index < kMaximumIntentWireContainerEntries;
       ++index) {
    leaves.append(index);
  }
  QVariantMap wire;
  for (int branch = 0; branch < 5; ++branch) {
    wire.insert(QStringLiteral("branch%1").arg(branch), leaves);
  }
  QVERIFY(wireContainsSecrets(wire));
}

void NetworkRedactionTests::redactsQuotedAndMalformedCredentialAssignments() {
  const QString quoted = redactDiagnostic(QStringLiteral(
      "password=\"hunter 2\"; client-password:'secret phrase', ssid=Home"));
  QVERIFY(!quoted.contains(QStringLiteral("hunter")));
  QVERIFY(!quoted.contains(QStringLiteral("secret phrase")));
  QVERIFY(quoted.contains(QStringLiteral("password=<redacted>")));
  QVERIFY(quoted.contains(QStringLiteral("client-password=<redacted>")));
  QVERIFY(quoted.contains(QStringLiteral("ssid=Home")));

  const QString separators = redactDiagnostic(
      QStringLiteral("wifi.password = alpha beta wifi_psk: gamma ssid=Home"));
  QVERIFY(!separators.contains(QStringLiteral("alpha")));
  QVERIFY(!separators.contains(QStringLiteral("beta")));
  QVERIFY(!separators.contains(QStringLiteral("gamma")));
  QVERIFY(separators.contains(QStringLiteral("ssid=Home")));

  const QString malformed =
      redactDiagnostic(QStringLiteral("error password='unterminated leak"));
  QVERIFY(!malformed.contains(QStringLiteral("unterminated")));
  QVERIFY(!malformed.contains(QStringLiteral("leak")));
  QCOMPARE(malformed, QStringLiteral("error password=<redacted>"));
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
  QVERIFY(!cleaned.contains(QStringLiteral("leak")));
  QCOMPARE(cleaned, QStringLiteral("network diagnostic withheld"));

  const QString huge(kMaxDiagnosticUtf8Bytes * 2, u'x');
  const QString bounded = redactDiagnostic(huge);
  QVERIFY(bounded.toUtf8().size() <= kMaxDiagnosticUtf8Bytes);
  QVERIFY(bounded.endsWith(QStringLiteral("…")));

  const QString unicode = redactDiagnostic(
      QString(kMaxDiagnosticUtf8Bytes, QChar(0x20ac)));
  QVERIFY(unicode.toUtf8().size() <= kMaxDiagnosticUtf8Bytes);
  QVERIFY(unicode.endsWith(QStringLiteral("…")));

  const QString spoof = redactDiagnostic(QStringLiteral("safe\u202eleak"));
  QVERIFY(!spoof.contains(QChar(0x202e)));
  QVERIFY(!spoof.contains(QStringLiteral("leak")));
  QVERIFY(isPresentationSafeText(spoof));
}

QTEST_MAIN(NetworkRedactionTests)
#include "tst_network_redaction.moc"

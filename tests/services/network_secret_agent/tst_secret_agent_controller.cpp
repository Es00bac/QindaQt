// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/secret_agent_controller.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>

using namespace QindaQt::Network::SecretAgent;

namespace {

QStringList *activeDiagnostics = nullptr;

void captureDiagnostic(QtMsgType, const QMessageLogContext &,
                       const QString &message) {
  if (activeDiagnostics != nullptr) {
    activeDiagnostics->append(message);
  }
}

class DiagnosticCapture final {
public:
  DiagnosticCapture() : m_previous(qInstallMessageHandler(captureDiagnostic)) {
    Q_ASSERT(activeDiagnostics == nullptr);
    activeDiagnostics = &messages;
  }

  ~DiagnosticCapture() {
    activeDiagnostics = nullptr;
    qInstallMessageHandler(m_previous);
  }

  DiagnosticCapture(const DiagnosticCapture &) = delete;
  DiagnosticCapture &operator=(const DiagnosticCapture &) = delete;

  QStringList messages;

private:
  QtMessageHandler m_previous = nullptr;
};

class FakeAuthority final : public ConnectionAuthority {
public:
  bool known = true;
  QString expectedOwner = QStringLiteral(":1.10");

  bool isKnownConnection(const QString &path, const QString &owner) override {
    return known &&
           path ==
               QStringLiteral("/org/freedesktop/NetworkManager/Settings/7") &&
           owner == expectedOwner;
  }
};

class FakePrompt final : public PromptPort {
public:
  using PromptPort::PromptPort;

  bool show = true;
  QList<PromptRequest> requests;
  QHash<quint64, Completion> completions;
  QList<quint64> canceled;

  bool showPrompt(const PromptRequest &request,
                  Completion completion) override {
    if (!show) {
      return false;
    }
    requests.append(request);
    completions.insert(request.requestId, std::move(completion));
    return true;
  }

  void cancelPrompt(const quint64 requestId) override {
    canceled.append(requestId);
    completions.remove(requestId);
  }

  void submit(PromptResult result) {
    auto completion = completions.take(result.requestId);
    completion(std::move(result));
  }
};

GetSecretsRequest
request(QString setting = QStringLiteral("802-11-wireless-security")) {
  NmSettingsMap connection;
  connection.insert(QStringLiteral("connection"),
                    {{QStringLiteral("id"), QStringLiteral("Canary Network")},
                     {QStringLiteral("uuid"),
                      QStringLiteral("12345678-1234-4234-9234-123456789abc")}});
  connection.insert(QStringLiteral("802-11-wireless-security"),
                    {{QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk")}});
  connection.insert(QStringLiteral("802-1x"), {});
  return {QStringLiteral(":1.10"),
          QStringLiteral(":1.10"),
          connection,
          QStringLiteral("/org/freedesktop/NetworkManager/Settings/7"),
          std::move(setting),
          {},
          0x1U};
}

QVariantList variantList(const qsizetype size) {
  QVariantList values;
  values.reserve(size);
  for (qsizetype index = 0; index < size; ++index) {
    values.append(true);
  }
  return values;
}

QVariantMap variantMap(const qsizetype size) {
  QVariantMap values;
  for (qsizetype index = 0; index < size; ++index) {
    values.insert(QStringLiteral("k%1").arg(index, 3, 10, QLatin1Char('0')),
                  true);
  }
  return values;
}

QVariantHash variantHash(const qsizetype size) {
  QVariantHash values;
  values.reserve(size);
  for (qsizetype index = 0; index < size; ++index) {
    values.insert(QStringLiteral("k%1").arg(index, 3, 10, QLatin1Char('0')),
                  true);
  }
  return values;
}

QVariant nestedLists(QVariant leaf, const int count) {
  for (int index = 0; index < count; ++index) {
    leaf = QVariantList{std::move(leaf)};
  }
  return leaf;
}

} // namespace

class SecretAgentControllerTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void refusesNonInteractiveUnknownAndForeignRequests();
  void acceptsAccountedNestedConnectionValues();
  void refusesNestedOverBudgetConnectionBeforePrompt();
  void enforcesNestedContainerCountBounds_data();
  void enforcesNestedContainerCountBounds();
  void enforcesVariantDepthBounds_data();
  void enforcesVariantDepthBounds();
  void returnsOnlyRequestedFieldsAndStorageChoice();
  void refusesDuplicateAndMalformedPromptValues();
  void cancelsByKeyAndTimeout();
  void wipesSharedSecretAllocations();
  void capturesDiagnosticsAndNeverLogsCanary();
};

void SecretAgentControllerTest::
    refusesNonInteractiveUnknownAndForeignRequests() {
  FakePrompt prompt;
  FakeAuthority authority;
  SecretAgentController controller(prompt, authority);
  QList<SecretAgentResult> results;
  auto capture = [&results](const SecretAgentResult result, SecretReply reply) {
    reply.wipe();
    results.append(result);
  };

  auto candidate = request();
  candidate.flags = 0;
  QVERIFY(!controller.requestSecrets(candidate, capture));
  candidate = request();
  candidate.flags = 0x11U;
  QVERIFY(!controller.requestSecrets(candidate, capture));
  candidate = request();
  candidate.caller = QStringLiteral(":1.99");
  QVERIFY(!controller.requestSecrets(candidate, capture));
  candidate = request(QStringLiteral("vpn"));
  QVERIFY(!controller.requestSecrets(candidate, capture));
  candidate = request();
  candidate.hints = {QStringLiteral("private-key")};
  QVERIFY(!controller.requestSecrets(candidate, capture));
  authority.known = false;
  candidate = request();
  QVERIFY(!controller.requestSecrets(candidate, capture));

  QCOMPARE(results, QList<SecretAgentResult>(6, SecretAgentResult::NoSecrets));
  QVERIFY(prompt.requests.isEmpty());
}

void SecretAgentControllerTest::
    acceptsAccountedNestedConnectionValues() {
  FakePrompt prompt;
  FakeAuthority authority;
  QList<SecretAgentResult> results;
  SecretAgentController controller(prompt, authority);
  auto candidate = request();
  candidate.connection[QStringLiteral("802-11-wireless-security")]
                      [QStringLiteral("vendor-payload")] =
      QVariantList{QByteArray("bounded"),
                   QVariantMap{{QStringLiteral("enabled"), true}}};

  QVERIFY(controller.requestSecrets(
      candidate, [&results](const SecretAgentResult result, SecretReply reply) {
        reply.wipe();
        results.append(result);
      }));
  QCOMPARE(prompt.requests.size(), 1);
  controller.cancel(candidate.connectionPath, candidate.settingName);
  QCOMPARE(results, {SecretAgentResult::UserCanceled});
}

void SecretAgentControllerTest::
    refusesNestedOverBudgetConnectionBeforePrompt() {
  // AGENT-NOTE: Raman P1-2 requires the aggregate bound to traverse nested
  // a{sv} containers; the 81,920-byte payload was accepted by f06d2fd.
  FakePrompt prompt;
  FakeAuthority authority;
  QList<SecretAgentResult> results;
  SecretAgentController controller(prompt, authority);
  auto candidate = request();
  QVariantList nested;
  for (int index = 0; index < 80; ++index) {
    nested.append(QByteArray(1'024, static_cast<char>('a' + (index % 26))));
  }
  candidate.connection[QStringLiteral("802-11-wireless-security")]
                      [QStringLiteral("vendor-payload")] = nested;

  QVERIFY(!controller.requestSecrets(
      candidate, [&results](const SecretAgentResult result, SecretReply reply) {
        reply.wipe();
        results.append(result);
      }));
  QCOMPARE(results, {SecretAgentResult::NoSecrets});
  QVERIFY(prompt.requests.isEmpty());
}

void SecretAgentControllerTest::enforcesNestedContainerCountBounds_data() {
  QTest::addColumn<QVariant>("payload");
  QTest::addColumn<bool>("accepted");

  QTest::newRow("variant-list-256") << QVariant(variantList(256)) << true;
  QTest::newRow("variant-list-257") << QVariant(variantList(257)) << false;
  QTest::newRow("variant-map-256") << QVariant(variantMap(256)) << true;
  QTest::newRow("variant-map-257") << QVariant(variantMap(257)) << false;
  QTest::newRow("variant-hash-256") << QVariant(variantHash(256)) << true;
  QTest::newRow("variant-hash-257") << QVariant(variantHash(257)) << false;
  QTest::newRow("string-list-256")
      << QVariant(QStringList(256, QString())) << true;
  QTest::newRow("string-list-257")
      << QVariant(QStringList(257, QString())) << false;
}

void SecretAgentControllerTest::enforcesNestedContainerCountBounds() {
  QFETCH(QVariant, payload);
  QFETCH(bool, accepted);
  FakePrompt prompt;
  FakeAuthority authority;
  QList<SecretAgentResult> results;
  SecretAgentController controller(prompt, authority);
  auto candidate = request();
  candidate.connection[QStringLiteral("802-11-wireless-security")]
                      [QStringLiteral("vendor-payload")] = std::move(payload);

  const bool admitted = controller.requestSecrets(
      candidate, [&results](const SecretAgentResult result, SecretReply reply) {
        reply.wipe();
        results.append(result);
      });
  QCOMPARE(admitted, accepted);
  if (accepted) {
    QCOMPARE(prompt.requests.size(), 1);
    controller.cancel(candidate.connectionPath, candidate.settingName);
  } else {
    QVERIFY(prompt.requests.isEmpty());
  }
  QCOMPARE(results.size(), 1);
  QCOMPARE(results.first(), accepted ? SecretAgentResult::UserCanceled
                                     : SecretAgentResult::NoSecrets);
}

void SecretAgentControllerTest::enforcesVariantDepthBounds_data() {
  QTest::addColumn<QVariant>("payload");
  QTest::addColumn<bool>("accepted");

  // AGENT-GUARD: Retain both sides of each policy comparison. The proof table
  // promises exact maximum-depth behavior, not merely rejection of deep input.
  QTest::newRow("variant-depth-8")
      << nestedLists(QByteArray("leaf"), 8) << true;
  QTest::newRow("variant-depth-9")
      << nestedLists(QByteArray("leaf"), 9) << false;
  QTest::newRow("string-list-depth-7")
      << nestedLists(QStringList{QStringLiteral("leaf")}, 7) << true;
  QTest::newRow("string-list-depth-8")
      << nestedLists(QStringList{QStringLiteral("leaf")}, 8) << false;
}

void SecretAgentControllerTest::enforcesVariantDepthBounds() {
  QFETCH(QVariant, payload);
  QFETCH(bool, accepted);
  FakePrompt prompt;
  FakeAuthority authority;
  QList<SecretAgentResult> results;
  SecretAgentController controller(prompt, authority);
  auto candidate = request();
  candidate.connection[QStringLiteral("802-11-wireless-security")]
                      [QStringLiteral("vendor-payload")] = std::move(payload);

  const bool admitted = controller.requestSecrets(
      candidate, [&results](const SecretAgentResult result, SecretReply reply) {
        reply.wipe();
        results.append(result);
      });
  QCOMPARE(admitted, accepted);
  if (accepted) {
    QCOMPARE(prompt.requests.size(), 1);
    controller.cancel(candidate.connectionPath, candidate.settingName);
  } else {
    QVERIFY(prompt.requests.isEmpty());
  }
  QCOMPARE(results.size(), 1);
  QCOMPARE(results.first(), accepted ? SecretAgentResult::UserCanceled
                                     : SecretAgentResult::NoSecrets);
}

void SecretAgentControllerTest::returnsOnlyRequestedFieldsAndStorageChoice() {
  FakePrompt prompt;
  FakeAuthority authority;
  SecretAgentController controller(prompt, authority);
  SecretAgentResult outcome = SecretAgentResult::NoSecrets;
  SecretReply reply;
  auto candidate = request();
  candidate.hints = {QStringLiteral("psk")};
  QVERIFY(controller.requestSecrets(
      candidate,
      [&outcome, &reply](const SecretAgentResult result, SecretReply value) {
        outcome = result;
        reply = std::move(value);
      }));
  QCOMPARE(prompt.requests.size(), 1);
  QCOMPARE(prompt.requests.first().fields.size(), 1);
  QCOMPARE(prompt.requests.first().fields.first().key, QStringLiteral("psk"));
  const quint64 id = prompt.requests.first().requestId;
  prompt.submit(
      {id, true, true, {{QStringLiteral("psk"), QByteArray("correct horse")}}});
  QCOMPARE(outcome, SecretAgentResult::Replied);
  QCOMPARE(reply.settingName, QStringLiteral("802-11-wireless-security"));
  QCOMPARE(reply.values.size(), 1);
  QCOMPARE(reply.values.first().bytes, QByteArray("correct horse"));
  QVERIFY(reply.remember);
  QCOMPARE(controller.storageDisposition(),
           StorageDisposition::NetworkManagerOwnsStorage);
  reply.wipe();

  outcome = SecretAgentResult::NoSecrets;
  auto wep = request();
  wep.connection[QStringLiteral("802-11-wireless-security")]
                [QStringLiteral("key-mgmt")] = QStringLiteral("none");
  wep.hints = {QStringLiteral("wep-key2")};
  QVERIFY(controller.requestSecrets(
      wep, [&outcome](const SecretAgentResult result, SecretReply value) {
        outcome = result;
        QCOMPARE(value.values.first().key, QStringLiteral("wep-key2"));
        value.wipe();
      }));
  const quint64 wepId = prompt.requests.last().requestId;
  prompt.submit({wepId,
                 true,
                 false,
                 {{QStringLiteral("wep-key2"), QByteArray("abcde")}}});
  QCOMPARE(outcome, SecretAgentResult::Replied);

  outcome = SecretAgentResult::NoSecrets;
  auto enterprise = request(QStringLiteral("802-1x"));
  enterprise.hints = {QStringLiteral("identity"), QStringLiteral("password")};
  QVERIFY(controller.requestSecrets(
      enterprise,
      [&outcome](const SecretAgentResult result, SecretReply value) {
        outcome = result;
        QCOMPARE(value.values.size(), 2);
        QVERIFY(!value.remember);
        value.wipe();
      }));
  const quint64 enterpriseId = prompt.requests.last().requestId;
  prompt.submit({enterpriseId,
                 true,
                 false,
                 {{QStringLiteral("identity"), QByteArray("etta")},
                  {QStringLiteral("password"), QByteArray("enterprise")}}});
  QCOMPARE(outcome, SecretAgentResult::Replied);
}

void SecretAgentControllerTest::refusesDuplicateAndMalformedPromptValues() {
  FakePrompt prompt;
  FakeAuthority authority;
  SecretAgentController controller(prompt, authority);
  QList<SecretAgentResult> results;
  const auto capture = [&results](const SecretAgentResult result,
                                  SecretReply reply) {
    reply.wipe();
    results.append(result);
  };

  auto enterprise = request(QStringLiteral("802-1x"));
  QVERIFY(controller.requestSecrets(enterprise, capture));
  const quint64 duplicateId = prompt.requests.last().requestId;
  prompt.submit({duplicateId,
                 true,
                 false,
                 {{QStringLiteral("identity"), QByteArray("etta")},
                  {QStringLiteral("identity"), QByteArray("duplicate")}}});
  QCOMPARE(results.last(), SecretAgentResult::NoSecrets);

  QVERIFY(controller.requestSecrets(request(), capture));
  const quint64 malformedId = prompt.requests.last().requestId;
  prompt.submit({malformedId,
                 true,
                 false,
                 {{QStringLiteral("psk"), QByteArray("\xff", 1)}}});
  QCOMPARE(results.last(), SecretAgentResult::NoSecrets);
}

void SecretAgentControllerTest::cancelsByKeyAndTimeout() {
  FakePrompt prompt;
  FakeAuthority authority;
  SecretAgentController controller(prompt, authority, 100);
  QList<SecretAgentResult> results;
  QVERIFY(controller.requestSecrets(
      request(), [&results](const SecretAgentResult result, SecretReply value) {
        value.wipe();
        results.append(result);
      }));
  const quint64 canceledId = prompt.requests.last().requestId;
  controller.cancel(
      QStringLiteral("/org/freedesktop/NetworkManager/Settings/7"),
      QStringLiteral("802-11-wireless-security"));
  QCOMPARE(results, {SecretAgentResult::UserCanceled});
  QCOMPARE(prompt.canceled, {canceledId});
  QCOMPARE(controller.pendingCount(), 0);

  QVERIFY(controller.requestSecrets(
      request(), [&results](const SecretAgentResult result, SecretReply value) {
        value.wipe();
        results.append(result);
      }));
  QTRY_COMPARE_WITH_TIMEOUT(results.size(), 2, 1'000);
  QCOMPARE(results.last(), SecretAgentResult::UserCanceled);
  QCOMPARE(controller.pendingCount(), 0);
}

void SecretAgentControllerTest::wipesSharedSecretAllocations() {
  // AGENT-NOTE: Raman P1-1 reproduced that f06d2fd detached before filling,
  // leaving every alias of the actual QString/QByteArray allocation intact.
  SecretValue value{QStringLiteral("psk"), QByteArray("canary-S3cr3t")};
  const QByteArray byteAlias = value.bytes;
  const char *buffer = value.bytes.constData();
  const qsizetype length = value.bytes.size();
  value.wipe();
  QVERIFY(value.bytes.isEmpty());
  for (qsizetype index = 0; index < length; ++index) {
    QCOMPARE(buffer[index], '\0');
    QCOMPARE(byteAlias.at(index), '\0');
  }

  const QString directSecret = QString::fromUtf8("canary-UTF16-S3cr3t");
  const QString nestedSecret = QString::fromUtf8("nested-canary-S3cr3t");
  const QByteArray nestedBytes("nested-byte-canary");
  NmSettingsMap settings{
      {QStringLiteral("802-11-wireless-security"),
       {{QStringLiteral("psk"), directSecret},
        {QStringLiteral("vendor-secrets"),
         QVariantList{nestedSecret, nestedBytes}}}}};
  wipeSettingsMap(settings);
  QVERIFY(settings.isEmpty());
  QVERIFY(std::all_of(directSecret.cbegin(), directSecret.cend(),
                      [](const QChar character) { return character.isNull(); }));
  QVERIFY(std::all_of(nestedSecret.cbegin(), nestedSecret.cend(),
                      [](const QChar character) { return character.isNull(); }));
  QVERIFY(std::all_of(nestedBytes.cbegin(), nestedBytes.cend(),
                      [](const char byte) { return byte == '\0'; }));

  QVariant editorValue = QString::fromUtf8("qml-editor-canary");
  const QString editorAlias = editorValue.toString();
  QCOMPARE(takeSecretUtf8(editorValue), QByteArray("qml-editor-canary"));
  QVERIFY(!editorValue.isValid());
  QVERIFY(std::all_of(editorAlias.cbegin(), editorAlias.cend(),
                      [](const QChar character) { return character.isNull(); }));
}

void SecretAgentControllerTest::capturesDiagnosticsAndNeverLogsCanary() {
  // AGENT-NOTE: Raman P1-3 found f06d2fd's empty handler captured nothing and
  // the assertion scanned test-authored text. Prove this real channel first.
  DiagnosticCapture diagnostics;
  qWarning("diagnostic-capture-canary");
  QCOMPARE(diagnostics.messages,
           QStringList{QStringLiteral("diagnostic-capture-canary")});
  diagnostics.messages.clear();

  FakePrompt prompt;
  FakeAuthority authority;
  SecretAgentController controller(prompt, authority);
  QVERIFY(controller.requestSecrets(
      request(), [](SecretAgentResult, SecretReply reply) {
        reply.wipe();
      }));
  const quint64 id = prompt.requests.first().requestId;
  prompt.submit({id,
                 true,
                 false,
                 {{QStringLiteral("psk"), QByteArray("canary-S3cr3t")}}});
  QVERIFY(std::none_of(diagnostics.messages.cbegin(),
                       diagnostics.messages.cend(),
                       [](const QString &line) {
                         return line.contains(QStringLiteral("canary-S3cr3t"));
                       }));
}

QTEST_GUILESS_MAIN(SecretAgentControllerTest)
#include "tst_secret_agent_controller.moc"

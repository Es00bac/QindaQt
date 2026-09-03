// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/secret_agent_controller.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Network::SecretAgent;

namespace {

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

} // namespace

class SecretAgentControllerTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void refusesNonInteractiveUnknownAndForeignRequests();
  void returnsOnlyRequestedFieldsAndStorageChoice();
  void refusesDuplicateAndMalformedPromptValues();
  void cancelsByKeyAndTimeout();
  void wipesByteBuffersAndNeverLogsCanary();
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

void SecretAgentControllerTest::wipesByteBuffersAndNeverLogsCanary() {
  SecretValue value{QStringLiteral("psk"), QByteArray("canary-S3cr3t")};
  char *buffer = value.bytes.data();
  const qsizetype length = value.bytes.size();
  value.wipe();
  QVERIFY(value.bytes.isEmpty());
  for (qsizetype index = 0; index < length; ++index) {
    QCOMPARE(buffer[index], '\0');
  }

  FakePrompt prompt;
  FakeAuthority authority;
  SecretAgentController controller(prompt, authority);
  QStringList diagnostics;
  const auto previous = qInstallMessageHandler(
      [](QtMsgType, const QMessageLogContext &, const QString &) {});
  QVERIFY(controller.requestSecrets(
      request(), [&diagnostics](SecretAgentResult, SecretReply reply) {
        diagnostics.append(QStringLiteral("request completed"));
        reply.wipe();
      }));
  const quint64 id = prompt.requests.first().requestId;
  prompt.submit({id,
                 true,
                 false,
                 {{QStringLiteral("psk"), QByteArray("canary-S3cr3t")}}});
  qInstallMessageHandler(previous);
  QVERIFY(std::none_of(diagnostics.cbegin(), diagnostics.cend(),
                       [](const QString &line) {
                         return line.contains(QStringLiteral("canary-S3cr3t"));
                       }));
}

QTEST_GUILESS_MAIN(SecretAgentControllerTest)
#include "tst_secret_agent_controller.moc"

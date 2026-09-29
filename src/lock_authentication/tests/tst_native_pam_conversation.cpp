// SPDX-License-Identifier: GPL-3.0-or-later
#include "authentication.h"
#include "native_pam_transaction.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::LockAuthentication;
class FixtureConversation : public Conversation {
public:
  std::optional<std::string> exchange(MessageKind kind,
                                      std::string_view) override {
    ++prompts;
    seenKind = kind;
    if (cancel) {
      stopped = true;
      return std::nullopt;
    }
    return response;
  }
  bool cancelled() const override { return stopped; }
  std::string response = "fixture-response";
  bool cancel = false, stopped = false;
  int prompts = 0;
  MessageKind seenKind = MessageKind::Error;
};
class NativePamConversationTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void cases_data() {
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("account");
    QTest::addColumn<int>("outcome");
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<bool>("nulResponse");
    QTest::newRow("synthetic-success")
        << "permit" << "permit" << int(Outcome::Authenticated) << false
        << false;
    QTest::newRow("native-denial")
        << "deny" << "permit" << int(Outcome::Denied) << false << false;
    QTest::newRow("account-denial")
        << "permit" << "account-deny" << int(Outcome::AccountDenied) << false
        << false;
    QTest::newRow("cancel-conversation")
        << "permit" << "permit" << int(Outcome::Cancelled) << true << false;
    QTest::newRow("malformed-style") << "invalid-style" << "permit"
                                     << int(Outcome::Denied) << false << false;
    QTest::newRow("oversize-prompt")
        << "huge-prompt" << "permit" << int(Outcome::Denied) << false << false;
    QTest::newRow("oversize-count")
        << "too-many" << "permit" << int(Outcome::Denied) << false << false;
    QTest::newRow("embedded-nul")
        << "permit" << "permit" << int(Outcome::Denied) << false << true;
  }
  void cases() {
    QFETCH(QString, mode);
    QFETCH(QString, account);
    QFETCH(int, outcome);
    QFETCH(bool, cancel);
    QFETCH(bool, nulResponse);
    QTemporaryDir conf;
    QVERIFY(conf.isValid());
    QFile service(conf.filePath("qindaqt-lock"));
    QVERIFY(service.open(QIODevice::WriteOnly));
    const QByteArray module = QINDAQT_PRIVATE_PAM_MODULE_PATH;
    service.write("auth required " + module + " " + mode.toUtf8() +
                  "\naccount required " + module + " " + account.toUtf8() +
                  "\n");
    service.close();
    FixtureConversation conversation;
    NativePamTransaction pam(conf.path().toStdString());
    conversation.cancel = cancel;
    if (nulResponse) {
      conversation.response = std::string("fixture-response\0extra", 22);
    }
    QCOMPARE(int(authenticateSessionUser(pam, conversation, "fixture-user")),
             outcome);
    if (mode == "permit" || mode == "deny") {
      QCOMPARE(conversation.prompts, 1);
      QCOMPARE(conversation.seenKind, MessageKind::Secret);
    }
  }
};
QTEST_GUILESS_MAIN(NativePamConversationTest)
#include "tst_native_pam_conversation.moc"

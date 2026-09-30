// SPDX-License-Identifier: GPL-3.0-or-later
#include "attempt_coordinator.h"
#include "authentication.h"
#include <QtTest>
using namespace QindaQt::LockAuthentication;
class ScriptedConversation final : public Conversation {
public:
  std::optional<std::string> exchange(MessageKind, std::string_view) override {
    return std::nullopt;
  }
  bool cancelled() const override { return stopped; }
  bool stopped = false;
};
class ScriptedPam final : public PamTransaction {
public:
  bool start(std::string_view service, std::string_view user,
             Conversation &) override {
    seenService = service;
    seenUser = user;
    ++starts;
    return startsOK;
  }
  bool authenticate() override {
    ++authCalls;
    if (cancelDuringAuth) {
      conversation->stopped = true;
    }
    return authOK;
  }
  bool approveAccount() override {
    ++accountCalls;
    if (cancelDuringAccount) {
      conversation->stopped = true;
    }
    return accountOK;
  }
  bool startsOK = true, authOK = true, accountOK = true;
  bool cancelDuringAuth = false, cancelDuringAccount = false;
  int starts = 0, authCalls = 0, accountCalls = 0;
  std::string seenService, seenUser;
  ScriptedConversation *conversation = nullptr;
};
class AuthenticationTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void decisions_data() {
    QTest::addColumn<bool>("start");
    QTest::addColumn<bool>("auth");
    QTest::addColumn<bool>("account");
    QTest::addColumn<int>("result");
    QTest::addColumn<int>("accounts");
    QTest::newRow("start-unavailable")
        << false << true << true << int(Outcome::Unavailable) << 0;
    QTest::newRow("bad-password")
        << true << false << true << int(Outcome::Denied) << 0;
    QTest::newRow("account-denied")
        << true << true << false << int(Outcome::AccountDenied) << 1;
    QTest::newRow("authenticated-account")
        << true << true << true << int(Outcome::Authenticated) << 1;
  }
  void decisions() {
    QFETCH(bool, start);
    QFETCH(bool, auth);
    QFETCH(bool, account);
    QFETCH(int, result);
    QFETCH(int, accounts);
    ScriptedConversation conv;
    ScriptedPam pam;
    pam.conversation = &conv;
    pam.startsOK = start;
    pam.authOK = auth;
    pam.accountOK = account;
    QCOMPARE(int(authenticateSessionUser(pam, conv, "session-user")), result);
    QCOMPARE(pam.seenService, std::string("qindaqt-lock"));
    QCOMPARE(pam.seenUser, std::string("session-user"));
    QCOMPARE(pam.authCalls, start ? 1 : 0);
    QCOMPARE(pam.accountCalls, accounts);
  }
  void cancellation_data() {
    QTest::addColumn<int>("phase");
    QTest::newRow("before-start") << 0;
    QTest::newRow("during-auth") << 1;
    QTest::newRow("during-account") << 2;
  }
  void cancellation() {
    QFETCH(int, phase);
    ScriptedConversation conv;
    ScriptedPam pam;
    pam.conversation = &conv;
    conv.stopped = phase == 0;
    pam.cancelDuringAuth = phase == 1;
    pam.cancelDuringAccount = phase == 2;
    QCOMPARE(authenticateSessionUser(pam, conv, "session-user"),
             Outcome::Cancelled);
    QCOMPARE(pam.accountCalls, phase == 2 ? 1 : 0);
  }
  void requestAndEpochIsolation() {
    AttemptCoordinator coordinator;
    QVERIFY(!coordinator.begin());
    coordinator.enterLockedSession();
    const auto cancelled = coordinator.begin();
    QVERIFY(cancelled);
    QVERIFY(!coordinator.begin());
    coordinator.cancel();
    const auto current = coordinator.begin();
    QVERIFY(current);
    QCOMPARE(coordinator.complete(*cancelled, Outcome::Authenticated),
             Completion::Ignored);
    QVERIFY(coordinator.busy());
    QCOMPARE(coordinator.complete(*current, Outcome::AccountDenied),
             Completion::Retry);
    const auto previousEpoch = coordinator.begin();
    QVERIFY(previousEpoch);
    coordinator.enterLockedSession();
    const auto recovery = coordinator.begin();
    QVERIFY(recovery);
    QCOMPARE(coordinator.complete(*previousEpoch, Outcome::Authenticated),
             Completion::Ignored);
    QVERIFY(coordinator.busy());
    QCOMPARE(coordinator.complete(*recovery, Outcome::Authenticated),
             Completion::Unlock);
    QCOMPARE(coordinator.complete(*recovery, Outcome::Authenticated),
             Completion::Ignored);
    QVERIFY(!coordinator.begin());
  }
  void crashInvalidatesApproval() {
    AttemptCoordinator coordinator;
    coordinator.enterLockedSession();
    const auto attempt = coordinator.begin();
    QVERIFY(attempt);
    coordinator.leaveLockedSession();
    QCOMPARE(coordinator.complete(*attempt, Outcome::Authenticated),
             Completion::Ignored);
    QVERIFY(!coordinator.begin());
  }
  void invalidIdentity() {
    ScriptedConversation conv;
    ScriptedPam pam;
    pam.conversation = &conv;
    QCOMPARE(authenticateSessionUser(pam, conv, ""), Outcome::Unavailable);
    QCOMPARE(authenticateSessionUser(pam, conv, std::string("a\0b", 3)),
             Outcome::Unavailable);
    QCOMPARE(pam.starts, 0);
  }
};
QTEST_GUILESS_MAIN(AuthenticationTest)
#include "tst_authentication.moc"

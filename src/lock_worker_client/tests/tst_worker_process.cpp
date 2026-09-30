// SPDX-License-Identifier: GPL-3.0-or-later
#include "worker_process.h"
#include "process_protection.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::LockAuthentication;
using QindaQt::LockWorkerClient::WorkerProcess;
class WorkerProcessTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() {
    qRegisterMetaType<AttemptToken>(); qRegisterMetaType<Outcome>(); qRegisterMetaType<MessageKind>();
  }
  void process_data() {
    QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("account");
    QTest::addColumn<bool>("cancel"); QTest::addColumn<int>("expected");
    QTest::newRow("approval") << "permit" << "permit" << false << int(Outcome::Authenticated);
    QTest::newRow("keyring-prompt-fallback") << "permit" << "keyring-fallback" << false << int(Outcome::Authenticated);
    QTest::newRow("bad-password") << "deny" << "permit" << false << int(Outcome::Denied);
    QTest::newRow("account-denied") << "permit" << "account-deny" << false << int(Outcome::AccountDenied);
    QTest::newRow("cancel-at-prompt") << "permit" << "permit" << true << int(Outcome::Cancelled);
    QTest::newRow("kill-blocked-pam-module") << "block" << "permit" << false << int(Outcome::Unavailable);
  }
  void process() {
    QFETCH(QString, mode); QFETCH(QString, account); QFETCH(bool, cancel); QFETCH(int, expected);
    QTemporaryDir conf; QVERIFY(conf.isValid());
    QFile service(conf.filePath("qindaqt-lock")); QVERIFY(service.open(QIODevice::WriteOnly));
    const QByteArray module = QINDAQT_PRIVATE_PAM_MODULE_PATH;
    service.write("auth required " + module + " " + mode.toUtf8() + "\naccount required " + module + " " + account.toUtf8() + "\nsession optional " + module + " " + account.toUtf8() + "\n"); service.close();
    WorkerProcess worker(QStringLiteral(QINDAQT_PRIVATE_WORKER_PATH), conf.path(), mode == "block" ? 300 : 3000);
    QSignalSpy done(&worker, &WorkerProcess::completed), prompts(&worker, &WorkerProcess::prompt);
    bool insideBlockedModule = false;
    QObject::connect(&worker, &WorkerProcess::prompt, &worker, [&](MessageKind kind, const QString &) {
      if (kind == MessageKind::Information) { insideBlockedModule = true; return; }
      if (cancel) worker.cancel(); else QVERIFY(worker.respond(QStringLiteral("fixture-response")));
    });
    const AttemptToken token{71, 83}; QVERIFY(worker.start(token));
    QVERIFY(!worker.start({71, 84}));
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(qvariant_cast<AttemptToken>(done[0][0]), token);
    QCOMPARE(int(qvariant_cast<Outcome>(done[0][1])), expected);
    QVERIFY(!worker.active()); QVERIFY(!worker.respond(QStringLiteral("late-response")));
    if (mode == "block") QVERIFY(insideBlockedModule);
    else QVERIFY(!prompts.isEmpty());
    // Completion cannot be duplicated by delayed process/socket notifications.
    QTest::qWait(10); QCOMPARE(done.size(), 1);
  }
  void productionPathAndInjection() {
    QVERIFY(QindaQt::LockPlatform::rootManagedPath(QStringLiteral("/usr/bin/true")));
    QVERIFY(QindaQt::LockPlatform::trustedQtPaths());
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const auto alias = dir.filePath("fake-worker"); QVERIFY(QFile::link(QStringLiteral("/usr/bin/true"), alias));
    QVERIFY(!QindaQt::LockPlatform::rootManagedPath(alias));
    WorkerProcess production;
    QVERIFY(!production.start({0, 1}));
    QVERIFY(!production.start({1, 0}));
    QVERIFY(!production.respond(QStringLiteral("invented-success")));
  }
  void failureAndDestruction() {
    QTemporaryDir conf; QVERIFY(conf.isValid());
    WorkerProcess missing(QStringLiteral("/nonexistent/qindaqt-private-pam-worker"), conf.path(), 3000);
    QSignalSpy done(&missing, &WorkerProcess::completed);
    QVERIFY(missing.start({2, 3})); QTRY_COMPARE(done.size(), 1);
    QCOMPARE(qvariant_cast<Outcome>(done[0][1]), Outcome::Unavailable);
    QVERIFY(!missing.active());
    // Destructor terminates only this disposable owned fixture child; no PAM
    // system configuration is involved (the private directory has no service).
    { WorkerProcess owned(QStringLiteral(QINDAQT_PRIVATE_WORKER_PATH), conf.path(), 3000);
      QVERIFY(owned.start({5, 7})); }
  }
};
QTEST_GUILESS_MAIN(WorkerProcessTest)
#include "tst_worker_process.moc"

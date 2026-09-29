// SPDX-License-Identifier: GPL-3.0-or-later
#include "worker_wire.h"
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <cerrno>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace QindaQt::LockAuthentication;
class OwnedWorker {
public:
  QProcess process;
  ~OwnedWorker() {
    if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(3000); }
  }
  int start(const QString &configuration) {
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair)) return -1;
    const int child = pair[1];
    process.setProgram(QINDAQT_PRIVATE_WORKER_PATH);
    process.setArguments({configuration});
    QProcess::UnixProcessParameters parameters;
    parameters.flags = QProcess::UnixProcessFlag::CloseFileDescriptors |
                       QProcess::UnixProcessFlag::DisableCoreDumps;
    parameters.lowestFileDescriptorToClose = 4;
    process.setUnixProcessParameters(parameters);
    process.setChildProcessModifier([child] {
      if (dup2(child, 3) < 0 || fcntl(3, F_SETFD, 0) < 0) _exit(127);
    });
    process.start();
    const bool ok = process.waitForStarted(3000);
    close(child);
    if (!ok) { close(pair[0]); return -1; }
    return pair[0];
  }
};
class WorkerChannelTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void frames() {
    const WireFrame frame{WireKind::Response, {23, 41}, "fixture-response"};
    auto encoded = WorkerChannel::encode(frame);
    QVERIFY(encoded);
    auto decoded = WorkerChannel::decode(*encoded);
    QVERIFY(decoded);
    QCOMPARE(decoded->token, frame.token); QCOMPARE(decoded->payload, frame.payload);
    QCOMPARE(decoded->kind, frame.kind);
    for (const int offset : {0, 3, 4, 5}) {
      auto bad = *encoded; bad[std::size_t(offset)] = char(255);
      QVERIFY(!WorkerChannel::decode(bad));
    }
    auto huge = *encoded; huge[6] = char(255); huge[7] = char(255);
    QVERIFY(!WorkerChannel::decode(huge));
    QVERIFY(!WorkerChannel::encode({WireKind::Response, {0, 1}, "x"}));
    QVERIFY(!WorkerChannel::encode({WireKind::Response, {1, 1}, std::string(4097, 'x')}));
    QVERIFY(!WorkerChannel::encode({WireKind::Response, {1, 1}, std::string("a\0b", 3)}));
    QVERIFY(!WorkerChannel::decode(encoded->substr(0, encoded->size() - 1)));
  }
  void deadlineAndEof() {
    int pair[2]; QVERIFY(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    WorkerChannel timeout(pair[0], std::chrono::milliseconds(20));
    QVERIFY(!timeout.receive()); QVERIFY(timeout.failed()); close(pair[1]);
    QVERIFY(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    WorkerChannel eof(pair[0], std::chrono::seconds(1)); close(pair[1]);
    QVERIFY(!eof.receive()); QVERIFY(eof.failed());
    WorkerChannel invalid(-1, std::chrono::seconds(1)); QVERIFY(invalid.failed());
  }
  void worker_data() {
    QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("account");
    QTest::addColumn<QString>("responseKind"); QTest::addColumn<int>("outcome");
    QTest::newRow("authenticated-plus-account") << "permit" << "permit" << "response" << int(Outcome::Authenticated);
    QTest::newRow("bad-password") << "deny" << "permit" << "response" << int(Outcome::Denied);
    QTest::newRow("account-failed") << "permit" << "account-deny" << "response" << int(Outcome::AccountDenied);
    QTest::newRow("cancel") << "permit" << "permit" << "cancel" << int(Outcome::Cancelled);
    QTest::newRow("stale-response-token") << "permit" << "permit" << "stale" << int(Outcome::Cancelled);
    QTest::newRow("invalid-conversation-style") << "invalid-style" << "permit" << "none" << int(Outcome::Denied);
    QTest::newRow("oversize-prompt") << "huge-prompt" << "permit" << "none" << int(Outcome::Denied);
    QTest::newRow("message-budget") << "too-many" << "permit" << "none" << int(Outcome::Denied);
  }
  void worker() {
    QFETCH(QString, mode); QFETCH(QString, account); QFETCH(QString, responseKind); QFETCH(int, outcome);
    QTemporaryDir configuration; QVERIFY(configuration.isValid());
    QFile service(configuration.filePath("qindaqt-lock")); QVERIFY(service.open(QIODevice::WriteOnly));
    const QByteArray module = QINDAQT_PRIVATE_PAM_MODULE_PATH;
    service.write("auth required " + module + " " + mode.toUtf8() + "\naccount required " + module + " " + account.toUtf8() + "\n"); service.close();
    OwnedWorker worker; const int fd = worker.start(configuration.path()); QVERIFY(fd >= 0);
    WorkerChannel channel(fd, std::chrono::seconds(3));
    const AttemptToken token{19, 31}; QVERIFY(channel.send({WireKind::Begin, token, {}}));
    auto frame = channel.receive(); QVERIFY(frame);
    if (responseKind != "none") {
      QCOMPARE(frame->kind, WireKind::Secret); QCOMPARE(frame->token, token);
      // The disposable worker is dumpable=0 even to its ordinary-UID parent.
      errno = 0;
      QCOMPARE(ptrace(PTRACE_ATTACH, static_cast<pid_t>(worker.process.processId()), nullptr, nullptr), -1L);
      QCOMPARE(errno, EPERM);
      QVERIFY(channel.send({responseKind == "cancel" ? WireKind::Cancel : WireKind::Response,
                            responseKind == "stale" ? AttemptToken{18, 31} : token,
                            responseKind == "cancel" ? "" : "fixture-response"}));
      frame = channel.receive(); QVERIFY(frame);
    }
    QCOMPARE(frame->kind, WireKind::Result); QCOMPARE(frame->token, token);
    QCOMPARE(frame->payload, std::string(1, static_cast<char>('0' + outcome)));
    QVERIFY(worker.process.waitForFinished(3000)); QCOMPARE(worker.process.exitCode(), 0);
    QCOMPARE(worker.process.readAllStandardOutput(), QByteArray());
    QCOMPARE(worker.process.readAllStandardError(), QByteArray());
  }
  void workerEofAndIdentityInjection() {
    QTemporaryDir configuration; QVERIFY(configuration.isValid());
    { OwnedWorker worker; const int fd = worker.start(configuration.path()); QVERIFY(fd >= 0);
      close(fd); QVERIFY(worker.process.waitForFinished(3000)); QVERIFY(worker.process.exitCode() != 0); }
    { OwnedWorker worker; const int fd = worker.start(configuration.path()); QVERIFY(fd >= 0);
      WorkerChannel channel(fd, std::chrono::seconds(3));
      QVERIFY(channel.send({WireKind::Begin, {1, 1}, "attacker-account"}));
      QVERIFY(!channel.receive()); QVERIFY(worker.process.waitForFinished(3000)); QVERIFY(worker.process.exitCode() != 0); }
  }
};
QTEST_GUILESS_MAIN(WorkerChannelTest)
#include "tst_worker_channel.moc"

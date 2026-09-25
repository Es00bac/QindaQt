// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTest>

#include "process_runner.h"
#include "process_tree.h"

#include <optional>
#include <unistd.h>

using namespace QindaQt::QindaLutris;

// The production runner against harmless host programs (sh, sleep, printf,
// setsid): bounded, argv-only, never synchronous, and -- the ADR-0275 4b
// contract -- a cancel or timeout leaves NO process of the tree alive,
// including a grandchild that started its own session.
namespace {

struct Run {
  QProcessRunner runner;
  std::optional<ProcessRunResult> result;
  explicit Run(ProcessContainment containment = ProcessContainment::ProcessGroup)
      : runner(containment) {
    runner.setStopTimings(1500, 3000);
    QObject::connect(&runner, &ProcessRunner::finished,
                     [this](const ProcessRunResult &r) { result = r; });
  }
  bool go(const ProcessRunSpec &spec) {
    runner.start(spec);
    if (result.has_value()) {
      return false; // must never answer synchronously
    }
    return wait();
  }
  bool wait() { return QTest::qWaitFor([this] { return result.has_value(); }, 15000); }
};

ProcessRunSpec spec(const QString &program, const QStringList &arguments = {}) {
  ProcessRunSpec s;
  s.program = program;
  s.arguments = arguments;
  s.timeoutMs = 5000;
  return s;
}

// A unique sleep duration marks this test's processes in /proc.
QString marker() {
  return QStringLiteral("3%1.%2")
      .arg(QRandomGenerator::global()->bounded(100, 999))
      .arg(QRandomGenerator::global()->bounded(1000, 9999));
}

int processesWith(const QString &needle) {
  int count = 0;
  const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString &entry : entries) {
    bool numeric = false;
    entry.toLongLong(&numeric);
    if (!numeric) {
      continue;
    }
    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(entry));
    if (cmdline.open(QIODevice::ReadOnly) && cmdline.readAll().contains(needle.toUtf8())) {
      ++count;
    }
  }
  return count;
}

// sh with a plain grandchild, a grandchild in its own session (setsid) and a
// grandchild that ignores SIGTERM (so only the SIGKILL escalation ends it).
ProcessRunSpec treeSpec(const QString &mark) {
  return spec(QStringLiteral("sh"),
              {QStringLiteral("-c"),
               QStringLiteral("sleep %1 & setsid sleep %1 & (trap '' TERM; exec sleep %1) & wait")
                   .arg(mark)});
}

bool waitForProcesses(const QString &mark, int expected) {
  return QTest::qWaitFor([&] { return processesWith(mark) >= expected; }, 5000);
}

} // namespace

class tst_process_runner : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() {
    for (const char *tool : {"true", "false", "sleep", "printf", "sh", "setsid"}) {
      if (QStandardPaths::findExecutable(QString::fromLatin1(tool)).isEmpty()) {
        QSKIP("coreutils/util-linux tools are not installed on this host");
      }
    }
  }

  void exitCodesAreReported() {
    Run ok;
    QVERIFY(ok.go(spec(QStringLiteral("true"))));
    QVERIFY(ok.result->started);
    QCOMPARE(ok.result->exitCode, 0);
    Run bad;
    QVERIFY(bad.go(spec(QStringLiteral("false"))));
    QCOMPARE(bad.result->exitCode, 1);
  }

  void argumentsAreNotInterpretedByAShell() {
    Run run;
    QVERIFY(run.go(spec(QStringLiteral("printf"), {QStringLiteral("%s"), QStringLiteral("$HOME; echo pwned")})));
    QCOMPARE(run.result->standardOutput, QByteArray("$HOME; echo pwned"));
  }

  void environmentOverlaysApplyAndPythonIsStripped() {
    qputenv("PYTHONPATH", "/evil/session");
    ProcessRunSpec s = spec(QStringLiteral("sh"),
                            {QStringLiteral("-c"),
                             QStringLiteral("printf '%s|%s|%s' \"$QINDA_TEST\" \"$PYTHONPATH\" \"$PYTHONHOME\"")});
    s.environment.insert(QStringLiteral("QINDA_TEST"), QStringLiteral("overlay"));
    s.environment.insert(QStringLiteral("PYTHONHOME"), QStringLiteral("/evil/recipe"));
    Run run;
    QVERIFY(run.go(s));
    qunsetenv("PYTHONPATH");
    QCOMPARE(run.result->standardOutput, QByteArray("overlay||"));
  }

  void outputIsBounded() {
    ProcessRunSpec s = spec(QStringLiteral("printf"), {QStringLiteral("0123456789")});
    s.maxOutputBytes = 4;
    Run run;
    QVERIFY(run.go(s));
    QCOMPARE(run.result->standardOutput, QByteArray("0123"));
    QVERIFY(run.result->outputTruncated);
  }

  void missingProgramAndUnboundedSpecAreRefusedAsynchronously() {
    Run missing;
    ProcessRunSpec s = spec(QStringLiteral("/nonexistent/qindalutris-no-such-program"));
    QVERIFY(missing.go(s));
    QVERIFY(!missing.result->started);
    QVERIFY(!missing.result->error.isEmpty());
    Run unbounded;
    s = spec(QStringLiteral("true"));
    s.timeoutMs = 0;
    QVERIFY(unbounded.go(s));
    QVERIFY(unbounded.result->error.contains(QStringLiteral("time limit")));
  }

  void cancelStopsTheWholeTreeInTheFallback() {
    const QString mark = marker();
    Run run(ProcessContainment::ProcessGroup);
    QVERIFY(!run.runner.usesSystemdScope());
    run.runner.start(treeSpec(mark));
    QVERIFY(waitForProcesses(mark, 3));
    run.runner.cancel();
    QVERIFY(!run.result.has_value()); // reported only once the tree is gone
    QVERIFY(run.wait());
    QVERIFY(run.result->cancelled);
    QVERIFY2(run.result->treeStopped, qPrintable(run.result->stopDetail));
    QCOMPARE(processesWith(mark), 0);
  }

  void timeoutStopsTheWholeTreeInTheFallback() {
    const QString mark = marker();
    Run run(ProcessContainment::ProcessGroup);
    ProcessRunSpec s = treeSpec(mark);
    s.timeoutMs = 800;
    QVERIFY(run.go(s));
    QVERIFY(run.result->timedOut);
    QVERIFY(!run.result->cancelled);
    QVERIFY2(run.result->treeStopped, qPrintable(run.result->stopDetail));
    QCOMPARE(processesWith(mark), 0);
  }

  void systemdScopeStopsEvenAnOrphanedSession() {
    // The isolated test environment hides the user manager; reach the real
    // one for this row only, and skip plainly when there is none.
    const QByteArray savedRuntime = qgetenv("XDG_RUNTIME_DIR");
    const QByteArray savedBus = qgetenv("DBUS_SESSION_BUS_ADDRESS");
    qputenv("XDG_RUNTIME_DIR", QByteArray("/run/user/") + QByteArray::number(::getuid()));
    qunsetenv("DBUS_SESSION_BUS_ADDRESS");
    const auto restore = qScopeGuard([&] {
      qputenv("XDG_RUNTIME_DIR", savedRuntime);
      qputenv("DBUS_SESSION_BUS_ADDRESS", savedBus);
    });
    Run run(ProcessContainment::SystemdScope);
    if (!run.runner.usesSystemdScope()) {
      QSKIP("no systemd user manager answers `systemctl --user is-system-running` here; "
            "the scope path is covered only where one runs");
    }
    const QString mark = marker();
    // The orphan's parent subshell exits at once, so it is reparented away
    // from the tree before anything could track it: only the scope holds it.
    ProcessRunSpec s = spec(QStringLiteral("sh"),
                            {QStringLiteral("-c"),
                             QStringLiteral("(setsid sleep %1 &); printf %s \"$QINDA_TEST\"; "
                                            "sleep %1 & wait")
                                 .arg(mark)});
    s.environment.insert(QStringLiteral("QINDA_TEST"), QStringLiteral("scoped"));
    run.runner.start(s);
    QVERIFY(waitForProcesses(mark, 2));
    const QString unit = run.runner.scopeUnit();
    QVERIFY(unit.startsWith(QStringLiteral("qindalutris-job-")));
    QVERIFY(isScopeActive(detectUserScopeTools(), unit));
    run.runner.cancel();
    QVERIFY(run.wait());
    QVERIFY(run.result->cancelled);
    QVERIFY2(run.result->treeStopped, qPrintable(run.result->stopDetail));
    QCOMPARE(run.result->standardOutput, QByteArray("scoped"));
    QCOMPARE(processesWith(mark), 0);
    QVERIFY(!isScopeActive(detectUserScopeTools(), unit));
  }

  void cancelWhenIdleIsANoOp() {
    Run run;
    run.runner.cancel();
    QTest::qWait(50);
    QVERIFY(!run.result.has_value());
  }

  void scopeCommandShapes() {
    QCOMPARE(systemdRunScopeArguments(QStringLiteral("qindalutris-job-x"), QStringLiteral("/usr/bin/umu-run"),
                                      {QStringLiteral("a b")}),
             QStringList({QStringLiteral("--user"), QStringLiteral("--scope"), QStringLiteral("--quiet"),
                          QStringLiteral("--collect"), QStringLiteral("--unit=qindalutris-job-x"),
                          QStringLiteral("--"), QStringLiteral("/usr/bin/umu-run"), QStringLiteral("a b")}));
    QCOMPARE(systemctlKillArguments(QStringLiteral("u"), QStringLiteral("SIGKILL")),
             QStringList({QStringLiteral("--user"), QStringLiteral("kill"), QStringLiteral("--signal=SIGKILL"),
                          QStringLiteral("u.scope")}));
    const QString name = newScopeUnitName();
    QVERIFY(name.startsWith(QStringLiteral("qindalutris-job-")));
    QCOMPARE(name.size(), QStringLiteral("qindalutris-job-").size() + 32);
    QVERIFY(newScopeUnitName() != name);
  }
};

QTEST_GUILESS_MAIN(tst_process_runner)
#include "tst_process_runner.moc"

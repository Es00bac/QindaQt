// SPDX-License-Identifier: GPL-3.0-or-later
#include <QStandardPaths>
#include <QTest>

#include "process_runner.h"

#include <optional>

using namespace QindaQt::QindaLutris;

// The production runner against harmless host programs (true, false, sleep,
// printf from coreutils): bounded, argv-only, and never synchronous.
namespace {

struct Run {
  QProcessRunner runner;
  std::optional<ProcessRunResult> result;
  Run() {
    QObject::connect(&runner, &ProcessRunner::finished,
                     [this](const ProcessRunResult &r) { result = r; });
  }
  bool go(const ProcessRunSpec &spec) {
    runner.start(spec);
    if (result.has_value()) {
      return false; // must never answer synchronously
    }
    return QTest::qWaitFor([this] { return result.has_value(); }, 10000);
  }
};

ProcessRunSpec spec(const QString &program, const QStringList &arguments = {}) {
  ProcessRunSpec s;
  s.program = QStandardPaths::findExecutable(program);
  s.arguments = arguments;
  s.timeoutMs = 5000;
  return s;
}

} // namespace

class tst_process_runner : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() {
    for (const char *tool : {"true", "false", "sleep", "printf", "sh"}) {
      if (QStandardPaths::findExecutable(QString::fromLatin1(tool)).isEmpty()) {
        QSKIP("coreutils are not installed on this host");
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
    QVERIFY(bad.result->started);
    QCOMPARE(bad.result->exitCode, 1);
  }

  void argumentsAreNotInterpretedByAShell() {
    Run run;
    QVERIFY(run.go(spec(QStringLiteral("printf"), {QStringLiteral("%s"), QStringLiteral("$HOME; echo pwned")})));
    QCOMPARE(run.result->standardOutput, QByteArray("$HOME; echo pwned"));
  }

  void environmentOverlaysApply() {
    ProcessRunSpec s = spec(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("printf %s \"$QINDA_TEST\"")});
    s.environment.insert(QStringLiteral("QINDA_TEST"), QStringLiteral("overlay"));
    Run run;
    QVERIFY(run.go(s));
    QCOMPARE(run.result->standardOutput, QByteArray("overlay"));
  }

  void outputIsBounded() {
    ProcessRunSpec s = spec(QStringLiteral("printf"), {QStringLiteral("0123456789")});
    s.maxOutputBytes = 4;
    Run run;
    QVERIFY(run.go(s));
    QCOMPARE(run.result->standardOutput, QByteArray("0123"));
    QVERIFY(run.result->outputTruncated);
  }

  void timeoutStopsTheProgram() {
    ProcessRunSpec s = spec(QStringLiteral("sleep"), {QStringLiteral("30")});
    s.timeoutMs = 200;
    Run run;
    QVERIFY(run.go(s));
    QVERIFY(run.result->timedOut);
    QVERIFY(!run.result->crashed);
    QVERIFY(!run.result->error.isEmpty());
  }

  void missingProgramFailsToStartAsynchronously() {
    ProcessRunSpec s;
    s.program = QStringLiteral("/nonexistent/qindalutris-no-such-program");
    s.timeoutMs = 1000;
    Run run;
    QVERIFY(run.go(s));
    QVERIFY(!run.result->started);
    QVERIFY(!run.result->error.isEmpty());
  }

  void unboundedSpecIsRefused() {
    ProcessRunSpec s = spec(QStringLiteral("true"));
    s.timeoutMs = 0;
    Run run;
    QVERIFY(run.go(s));
    QVERIFY(!run.result->started);
    QVERIFY(run.result->error.contains(QStringLiteral("time limit")));
  }

  void cancelKillsAndSuppressesTheResult() {
    Run run;
    run.runner.start(spec(QStringLiteral("sleep"), {QStringLiteral("30")}));
    run.runner.cancel();
    QTest::qWait(100);
    QVERIFY(!run.result.has_value());
  }
};

QTEST_GUILESS_MAIN(tst_process_runner)
#include "tst_process_runner.moc"

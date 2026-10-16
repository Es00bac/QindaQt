// SPDX-License-Identifier: GPL-3.0-or-later
// The owner's scenario, end to end and without a compositor session: the REAL
// private greeter binary (real Qt Wayland client and session-lock integration,
// real QtQuick/QML, real NativeWindows, input filter and controller, real PAM
// worker over a private PAM configuration with a 3 s whole-attempt deadline)
// against a fake ext-session-lock compositor. It proves, in one process tree,
// that an idle lock starts no PAM conversation, that the first key press starts
// one, that its timeout returns the greeter to a ready state by itself, that the
// next key press brings the prompt back (previously the field was dead until
// Unlock was clicked) and that an authenticated response unlocks (ADR-0382).
#include "fake_lock_compositor.h"
#include <QProcess>
#include <QtTest>
#include <cstdlib>
#include <filesystem>
using QindaQt::LockProtocol::Testing::FakeLockCompositor;
using Kind = FakeLockCompositor::Observation::Kind;

class GreeterProcessTest : public QObject {
  Q_OBJECT
  FakeLockCompositor m_compositor;
  QProcess m_greeter;
  QByteArray m_output, m_errors;
  std::string m_runtime;

  void pump() {
    m_output += m_greeter.readAllStandardOutput();
    m_errors += m_greeter.readAllStandardError();
  }
  bool seen(const char *marker) {
    pump();
    return m_output.contains(marker);
  }
  // Forgets earlier output so a marker is only matched when it is NEW.
  void forget() {
    pump();
    m_output.clear();
  }
  void send(const char *command) { QVERIFY(m_greeter.write(command) > 0); }
  const char *diagnostics() { pump(); return (m_output + "\n--- stderr ---\n" + m_errors).constData(); }

private Q_SLOTS:
  void initTestCase() {
    char runtimeTemplate[] = "/tmp/qindaqt-greeter-test-XXXXXX";
    QVERIFY(mkdtemp(runtimeTemplate));
    m_runtime = runtimeTemplate;
    qputenv("XDG_RUNTIME_DIR", m_runtime.c_str()); // 0700, as Qt and libwayland require.
    QVERIFY(m_compositor.start("qindaqt-greeter-test-0"));
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), QString::fromStdString(m_runtime));
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("qindaqt-greeter-test-0"));
    environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    environment.insert(QStringLiteral("QT_FATAL_WARNINGS"), QStringLiteral("0")); // A 0x0 configure warns on purpose.
    environment.remove(QStringLiteral("WAYLAND_SOCKET"));
    m_greeter.setProcessEnvironment(environment);
    m_greeter.setProgram(QStringLiteral(QINDAQT_PRIVATE_GREETER_PATH));
    m_greeter.start();
    QVERIFY2(m_greeter.waitForStarted(10000), "the private greeter did not start");
  }
  void cleanupTestCase() {
    if (m_greeter.state() != QProcess::NotRunning) { m_greeter.kill(); m_greeter.waitForFinished(3000); }
    m_compositor.stop();
    std::filesystem::remove_all(m_runtime);
  }
  void lockedGreeterIsReadyAndSilent() {
    // The greeter creates its lock surface; the compositor first sends an
    // unusable configure (which must not kill the process) and then a real one.
    QTRY_COMPARE_WITH_TIMEOUT(m_compositor.count(Kind::LockSurfaceRequested), std::size_t{1}, 20000);
    QVERIFY2(m_compositor.sendConfigure(0, 0) != 0, "no lock surface to configure");
    QTest::qWait(300);
    QVERIFY2(m_greeter.state() == QProcess::Running, diagnostics());
    QVERIFY(m_compositor.sendConfigure(1280, 1024) != 0);
    QTRY_VERIFY_WITH_TIMEOUT(m_compositor.count(Kind::Commit) >= 1, 15000);
    m_compositor.sendLocked();
    QTRY_VERIFY2_WITH_TIMEOUT(seen("NATIVE_GREETER_UI_READY"), diagnostics(), 15000);
    // Ready, but nobody touched anything: no PAM conversation exists.
    QTest::qWait(700);
    QVERIFY2(!seen("NATIVE_GREETER_PROMPT"), "a PAM prompt appeared without any input");
    QVERIFY(m_greeter.state() == QProcess::Running);
  }
  void theFirstKeyPressStartsAnAttempt() {
    forget();
    send("key\n"); // A real synthetic key press through window, input filter and QML.
    QTRY_VERIFY2_WITH_TIMEOUT(seen("NATIVE_GREETER_PROMPT"), diagnostics(), 10000);
  }
  void aTimeoutLeavesAReadyGreeterAndTheNextKeyBringsThePromptBack() {
    // Nobody types: the 3 s whole-attempt deadline kills the worker. The greeter
    // reports it and goes idle by itself ...
    forget();
    QTRY_VERIFY2_WITH_TIMEOUT(seen("NATIVE_GREETER_DENIED"), diagnostics(), 12000);
    QVERIFY(m_greeter.state() == QProcess::Running);
    // ... and the very next key press starts a fresh conversation. Before this
    // change nothing visible happened until the Unlock button was clicked.
    forget();
    send("key\n");
    QTRY_VERIFY2_WITH_TIMEOUT(seen("NATIVE_GREETER_PROMPT"), diagnostics(), 10000);
  }
  void anAuthenticatedResponseUnlocks() {
    forget();
    send("password\n"); // The fixture's synthetic response to the pending prompt.
    QTRY_COMPARE_WITH_TIMEOUT(m_compositor.count(Kind::UnlockRequested), std::size_t{1}, 15000);
    // waitForFinished() reports false for a process that already finished, so poll the state.
    QTRY_VERIFY2_WITH_TIMEOUT(m_greeter.state() == QProcess::NotRunning, diagnostics(), 15000);
    QCOMPARE(m_greeter.exitStatus(), QProcess::NormalExit);
    QCOMPARE(m_greeter.exitCode(), 0);
    QVERIFY2(seen("NATIVE_GREETER_FINISHED"), diagnostics());
  }
};
QTEST_GUILESS_MAIN(GreeterProcessTest)
#include "tst_lock_greeter_process.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellpreferencevalues.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

// Exercises the real qindaqt-shell startup path against a private Settings1
// service: saved profile selection recovery and explicit CLI failure.
// AGENT-NOTE: Qt 6.11 moc silently stops lexing at the R"xml(...)" literal in
// startPrivateDaemon below, so this Q_OBJECT class must stay declared before
// the anonymous namespace; moving the helper above the class erases the
// generated metaobject and fails the link with a missing vtable.
class ShellRuntimeStartupLaunchTests final : public QObject {
    Q_OBJECT

private slots:
    void savedDeletedProfileFallsBackToDefault();
    void explicitUnknownProfileIsAnError();
    void absentServiceUsesBuiltInDefaults();
};

namespace {

// AGENT-NOTE: dbus-daemon --session loads the host session configuration with
// its standard service directories, so a host-installed Settings1 provider is
// autoactivated and answers the shell's snapshot read, defeating the
// absent-service scenario. This explicit config declares no service
// directories, so nothing can be activated. Mirrors the private-bus fix in
// tests/shell/tst_shellstartuppreferences.cpp.
QString startPrivateDaemon(QProcess &daemon, QTemporaryDir &busDirectory)
{
    QFile config(busDirectory.filePath(QStringLiteral("dbus.conf")));
    if (!config.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    const QByteArray configContents = R"xml(<!DOCTYPE busconfig PUBLIC
        "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
        "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=)xml"
                         + busDirectory.path().toUtf8()
                         + R"xml(</listen>
  <policy context="default">
    <allow user="*"/>
    <allow own="*"/>
    <allow send_destination="*"/>
    <allow send_interface="*"/>
    <allow receive_sender="*"/>
  </policy>
</busconfig>
)xml";
    if (config.write(configContents) != configContents.size()) {
        return {};
    }
    config.close();
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--config-file"), config.fileName(),
                  QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    if (!daemon.waitForStarted() || !daemon.waitForReadyRead()) {
        return {};
    }
    return QString::fromUtf8(daemon.readLine()).trimmed();
}

QProcessEnvironment shellEnvironment(const QString &busAddress,
                                     const QString &xdgDataHome)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), busAddress);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    // Isolate the user profile/settings stores from the host session.
    environment.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    return environment;
}

struct ShellRun {
    int exitCode = -1;
    QString output;
};

ShellRun runShell(const QString &busAddress, const QString &xdgDataHome,
                  const QStringList &arguments)
{
    QProcess shell;
    shell.setProcessEnvironment(shellEnvironment(busAddress, xdgDataHome));
    shell.start(QStringLiteral(QINDAQT_SHELL_EXECUTABLE), arguments);
    if (!shell.waitForStarted()) {
        return {};
    }
    // AGENT-NOTE: waitForFinished would block this thread's event loop and
    // starve the in-process Settings1 service hosting the private bus; spin
    // the loop instead so the service can answer the shell's snapshot read.
    QElapsedTimer deadline;
    deadline.start();
    while (shell.state() != QProcess::NotRunning
           && deadline.elapsed() < 30'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (shell.state() != QProcess::NotRunning) {
        shell.kill();
        shell.waitForFinished();
        return {};
    }
    return {shell.exitCode(),
            QString::fromUtf8(shell.readAllStandardOutput())
                + QString::fromUtf8(shell.readAllStandardError())};
}

} // namespace

void ShellRuntimeStartupLaunchTests::savedDeletedProfileFallsBackToDefault()
{
    QTemporaryDir busDirectory(QStringLiteral("/tmp/qindaqt-launch-bus-XXXXXX"));
    QVERIFY(busDirectory.isValid());
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceConnection = QStringLiteral("launch-service-") + suffix;
    const QString writerConnection = QStringLiteral("launch-writer-") + suffix;
    QProcess daemon;
    // AGENT-GUARD: every early return above the explicit teardown must still
    // disconnect the named buses and reap the private daemon.
    const auto cleanup = qScopeGuard([&] {
        QDBusConnection::disconnectFromBus(serviceConnection);
        QDBusConnection::disconnectFromBus(writerConnection);
        if (daemon.state() != QProcess::NotRunning) {
            daemon.kill();
        }
        daemon.waitForFinished();
    });
    const QString address = startPrivateDaemon(daemon, busDirectory);
    QVERIFY(!address.isEmpty());
    auto serviceBus = QDBusConnection::connectToBus(address, serviceConnection);
    auto writerBus = QDBusConnection::connectToBus(address, writerConnection);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(writerBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"),
        nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"),
        nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ResidentSettingsService service(
        serviceBus, *active, *legacy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"),
        storage.filePath(QStringLiteral("user.json")));
    QVERIFY(service.start().ok());

    // The Customize route saved a profile that was deleted afterwards.
    {
        QtSettingsTransport transport(writerBus);
        SettingsClient writer(transport, ShellPreferenceValues::scopedKeys(),
                              {.requestTimeoutMilliseconds = 500,
                               .debounceMilliseconds = 0,
                               .retryMilliseconds = {10, 20}});
        QVERIFY2(writer.start(&error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(writer.state() == ClientState::Ready, 2'000);
        QVERIFY2(writer.setUserValue(QStringLiteral("panels.layoutProfile"),
                                     QStringLiteral("deleted-profile"), &error),
                 qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(!writer.writeInFlight(), 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            writer.snapshot()->values.value(QStringLiteral("panels.layoutProfile"))
                    .toString()
                == QStringLiteral("deleted-profile"),
            2'000);
    }

    QTemporaryDir xdg;
    QVERIFY(xdg.isValid());
    const ShellRun run = runShell(address, xdg.path(), {});
    // The offscreen platform exits 3 only after catalogs load and the profile
    // selection resolves; reaching it proves the saved selection recovered.
    QCOMPARE(run.exitCode, 3);
    QVERIFY2(run.output.contains(QStringLiteral("deleted-profile")),
             qPrintable(run.output));
    QVERIFY2(run.output.contains(
                 QStringLiteral("falling back to the default profile")),
             qPrintable(run.output));

    service.stop();
    QDBusConnection::disconnectFromBus(serviceConnection);
    QDBusConnection::disconnectFromBus(writerConnection);
    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

void ShellRuntimeStartupLaunchTests::explicitUnknownProfileIsAnError()
{
    QTemporaryDir busDirectory(QStringLiteral("/tmp/qindaqt-launch-bus-XXXXXX"));
    QVERIFY(busDirectory.isValid());
    QProcess daemon;
    // Reap the private daemon even when an assertion above the teardown fails.
    const auto cleanup = qScopeGuard([&] {
        if (daemon.state() != QProcess::NotRunning) {
            daemon.kill();
        }
        daemon.waitForFinished();
    });
    const QString address = startPrivateDaemon(daemon, busDirectory);
    QVERIFY(!address.isEmpty());

    QTemporaryDir xdg;
    QVERIFY(xdg.isValid());
    const ShellRun run = runShell(address, xdg.path(),
                                  {QStringLiteral("--profile"),
                                   QStringLiteral("deleted-profile")});
    QCOMPARE(run.exitCode, 2);
    QVERIFY2(run.output.contains(
                 QStringLiteral("Unknown profile: deleted-profile")),
             qPrintable(run.output));

    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

void ShellRuntimeStartupLaunchTests::absentServiceUsesBuiltInDefaults()
{
    QTemporaryDir busDirectory(QStringLiteral("/tmp/qindaqt-launch-bus-XXXXXX"));
    QVERIFY(busDirectory.isValid());
    QProcess daemon;
    // Reap the private daemon even when an assertion above the teardown fails.
    const auto cleanup = qScopeGuard([&] {
        if (daemon.state() != QProcess::NotRunning) {
            daemon.kill();
        }
        daemon.waitForFinished();
    });
    const QString address = startPrivateDaemon(daemon, busDirectory);
    QVERIFY(!address.isEmpty());

    QTemporaryDir xdg;
    QVERIFY(xdg.isValid());
    const ShellRun run = runShell(address, xdg.path(), {});
    QCOMPARE(run.exitCode, 3);
    QVERIFY2(run.output.contains(
                 QStringLiteral("no confirmed Settings1 preferences")),
             qPrintable(run.output));

    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

QTEST_GUILESS_MAIN(ShellRuntimeStartupLaunchTests)
#include "tst_shellruntime_startuplaunch.moc"

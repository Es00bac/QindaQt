// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/session_supervisor/direct_parent_process.h"
#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include "qindaqt/session_supervisor/session_service.h"
#include "qindaqt/session_supervisor/tokenized_process_launcher.h"

#include <QFile>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QProcess>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <cerrno>
#include <limits>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <utility>

using namespace QindaQt;

namespace {

class ChildSubreaperScope final {
public:
    ChildSubreaperScope()
    {
        m_active = ::prctl(PR_GET_CHILD_SUBREAPER, &m_previous) == 0
                   && ::prctl(PR_SET_CHILD_SUBREAPER, 1) == 0;
    }

    ~ChildSubreaperScope()
    {
        if (m_active) {
            ::prctl(PR_SET_CHILD_SUBREAPER, m_previous);
        }
    }

    [[nodiscard]] bool active() const noexcept { return m_active; }

private:
    int m_previous = 0;
    bool m_active = false;
};

std::optional<pid_t> startLifetimeParent(QProcess &parent, const QStringList &arguments,
                                         QString *error)
{
    parent.start(QStringLiteral(QINDAQT_SESSION_PARENT_DEATH_HELPER), arguments);
    if (!parent.waitForStarted(5'000) || !parent.waitForReadyRead(5'000)) {
        *error = parent.errorString();
        return std::nullopt;
    }
    const QString line = QString::fromUtf8(parent.readLine()).trimmed();
    const QStringList fields = line.split(QLatin1Char(' '));
    bool valid = false;
    const qint64 processId = fields.size() == 2 ? fields.at(1).toLongLong(&valid) : 0;
    if (!valid || fields.constFirst() != QLatin1String("ready")
        || !SessionSupervisor::isUsableCompositorProcessId(processId)) {
        *error = QStringLiteral("invalid lifetime-helper report: %1").arg(line);
        return std::nullopt;
    }
    return static_cast<pid_t>(processId);
}

std::optional<int> reapAfterParentDeath(pid_t childProcessId)
{
    constexpr int Attempts = 500;
    for (int attempt = 0; attempt < Attempts; ++attempt) {
        int status = 0;
        const pid_t result = ::waitpid(childProcessId, &status, WNOHANG);
        if (result == childProcessId) {
            return status;
        }
        if (result < 0 && errno != EINTR && errno != ECHILD) {
            return std::nullopt;
        }
        QThread::msleep(10);
    }
    return std::nullopt;
}

} // namespace

class SessionProcessSupervisorTests final : public QObject {
    Q_OBJECT

private slots:
    void launcherPassesASecretOnlyThroughTheDescriptor();
    void validatesCompositorProcessIdentifiers();
    void parentDeathTerminatesTheWitnessedSessionChild();
    void supervisorDeathTerminatesATokenizedChild();
    void buildsTheExactNonSecretShellArguments();
    void supervisorStartsBothChildrenAndCouplesTheirLifetime();
    void supervisorDoesNotRestartShellAfterHostExit();
    void supervisorRestartsShellOnceWithoutRestartingHost();
    void supervisorEndsSessionAfterReplacementShellExits();
    void supervisorEndsSessionWhenShellRestartCannotStart();
    void supervisorRollsBackWhenTheSecondChildCannotStart();
    void session1AuthenticatesShellAndStopsChildrenInOrder();
    void optionalSecretAgentRestartsOnceWithoutBlockingSession();
    void optionalWelcomeStartsAfterShellAndDoesNotRestart();
    void session1DescriptorMatchesTheFixedSurface();
};

void SessionProcessSupervisorTests::launcherPassesASecretOnlyThroughTheDescriptor()
{
    const auto token = Services::NotificationPresentation::PresentationAccessToken::generate();
    const QString secret = token.toHex();
    QProcess child;
    QString error;
    QVERIFY2(SessionSupervisor::TokenizedProcessLauncher::start(
                 child, QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER),
                 {QStringLiteral("--quick-exit")}, token, &error),
             qPrintable(error));
    QVERIFY(child.waitForFinished(5'000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
    QVERIFY(child.readAllStandardOutput().contains("token-channel-ok host"));
    QVERIFY(!child.arguments().join(QLatin1Char(' ')).contains(secret));
    const QString descriptor = child.arguments().constLast();
    bool valid = false;
    QVERIFY(descriptor.toInt(&valid) >= 3);
    QVERIFY(valid);
    QCOMPARE(child.arguments(),
             QStringList({QStringLiteral("--quick-exit"), QStringLiteral("--presentation-token-fd"),
                          descriptor}));
}

void SessionProcessSupervisorTests::validatesCompositorProcessIdentifiers()
{
    QVERIFY(!SessionSupervisor::isUsableCompositorProcessId(-1));
    QVERIFY(!SessionSupervisor::isUsableCompositorProcessId(0));
    QVERIFY(!SessionSupervisor::isUsableCompositorProcessId(1));
    QVERIFY(SessionSupervisor::isUsableCompositorProcessId(2));
    constexpr qint64 MaximumProcessId = static_cast<qint64>(std::numeric_limits<pid_t>::max());
    QVERIFY(SessionSupervisor::isUsableCompositorProcessId(MaximumProcessId));
    if constexpr (MaximumProcessId < std::numeric_limits<qint64>::max()) {
        QVERIFY(!SessionSupervisor::isUsableCompositorProcessId(MaximumProcessId + 1));
    }
}

void SessionProcessSupervisorTests::parentDeathTerminatesTheWitnessedSessionChild()
{
    ChildSubreaperScope subreaper;
    QVERIFY(subreaper.active());
    QProcess parent;
    QString error;
    const auto childProcessId =
        startLifetimeParent(parent, {QStringLiteral("--spawn-witness")}, &error);
    QVERIFY2(childProcessId.has_value(), qPrintable(error));
    parent.kill();
    QVERIFY(parent.waitForFinished(5'000));

    const auto status = reapAfterParentDeath(*childProcessId);
    QVERIFY(status.has_value());
    QVERIFY(WIFSIGNALED(*status));
    QCOMPARE(WTERMSIG(*status), SIGKILL);
}

void SessionProcessSupervisorTests::supervisorDeathTerminatesATokenizedChild()
{
    ChildSubreaperScope subreaper;
    QVERIFY(subreaper.active());
    QProcess parent;
    QString error;
    const auto childProcessId = startLifetimeParent(
        parent,
        {QStringLiteral("--spawn-token-child"), QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER)},
        &error);
    QVERIFY2(childProcessId.has_value(), qPrintable(error));
    parent.kill();
    QVERIFY(parent.waitForFinished(5'000));

    const auto status = reapAfterParentDeath(*childProcessId);
    QVERIFY(status.has_value());
    QVERIFY(WIFSIGNALED(*status));
    QCOMPARE(WTERMSIG(*status), SIGKILL);
}

void SessionProcessSupervisorTests::buildsTheExactNonSecretShellArguments()
{
    SessionSupervisor::SessionProcessOptions options;
    options.profileId = QStringLiteral("test-shell-role");
    options.themeId = QStringLiteral("test-theme");
    options.compositorProcessId = 42'424;
    QString error;
    const auto arguments = SessionSupervisor::shellProcessArguments(options, &error);
    QVERIFY2(arguments.has_value(), qPrintable(error));
    QCOMPARE(*arguments,
             QStringList({QStringLiteral("--profile"), QStringLiteral("test-shell-role"),
                          QStringLiteral("--theme"), QStringLiteral("test-theme"),
                          QStringLiteral("--compositor-pid"), QStringLiteral("42424")}));

    options.compositorProcessId = 0;
    QVERIFY(!SessionSupervisor::shellProcessArguments(options, &error).has_value());
    QVERIFY(!error.isEmpty());
}

void SessionProcessSupervisorTests::supervisorStartsBothChildrenAndCouplesTheirLifetime()
{
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-shell-role");
    options.themeId = QStringLiteral("test-theme");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy finished(&supervisor, &SessionSupervisor::SessionProcessSupervisor::finished);
    QSignalSpy restarted(&supervisor, &SessionSupervisor::SessionProcessSupervisor::shellRestarted);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QVERIFY(supervisor.isRunning());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5'000);
    QCOMPARE(restarted.size(), 1);
    QCOMPARE(supervisor.shellRestartCount(), 1);
    QCOMPARE(finished.constFirst().at(0).toInt(), 1);
    QVERIFY(!supervisor.isRunning());
}

void SessionProcessSupervisorTests::supervisorDoesNotRestartShellAfterHostExit()
{
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy restarted(&supervisor, &SessionSupervisor::SessionProcessSupervisor::shellRestarted);
    QSignalSpy finished(&supervisor, &SessionSupervisor::SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    const qint64 hostProcessId = supervisor.notificationHostProcessId();
    QVERIFY(hostProcessId > 1);

    QCOMPARE(::kill(static_cast<pid_t>(hostProcessId), SIGTERM), 0);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5'000);
    QCOMPARE(restarted.size(), 0);
    QCOMPARE(supervisor.shellRestartCount(), 0);
    QVERIFY(finished.constFirst().at(1).toString().contains(
        QStringLiteral("notification host exited")));
    QVERIFY(!supervisor.isRunning());
    QCOMPARE(supervisor.notificationHostProcessId(), 0);
    QCOMPARE(supervisor.shellProcessId(), 0);
}

void SessionProcessSupervisorTests::supervisorRestartsShellOnceWithoutRestartingHost()
{
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy restarted(&supervisor, &SessionSupervisor::SessionProcessSupervisor::shellRestarted);
    QSignalSpy finished(&supervisor, &SessionSupervisor::SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    const qint64 hostProcessId = supervisor.notificationHostProcessId();
    const qint64 shellProcessId = supervisor.shellProcessId();
    QVERIFY(hostProcessId > 1);
    QVERIFY(shellProcessId > 1);

    QCOMPARE(::kill(static_cast<pid_t>(shellProcessId), SIGTERM), 0);
    QTRY_COMPARE_WITH_TIMEOUT(restarted.size(), 1, 5'000);
    QCOMPARE(restarted.constFirst().at(0).toLongLong(), shellProcessId);
    const qint64 replacementProcessId = restarted.constFirst().at(1).toLongLong();
    QVERIFY(replacementProcessId > 1);
    QVERIFY(replacementProcessId != shellProcessId);
    QCOMPARE(supervisor.notificationHostProcessId(), hostProcessId);
    QCOMPARE(supervisor.shellProcessId(), replacementProcessId);
    QCOMPARE(supervisor.shellRestartCount(), 1);
    QVERIFY(supervisor.isRunning());
    QCOMPARE(finished.size(), 0);

    supervisor.stop();
    QVERIFY(!supervisor.isRunning());
    QCOMPARE(supervisor.notificationHostProcessId(), 0);
    QCOMPARE(supervisor.shellProcessId(), 0);
    QCOMPARE(supervisor.shellRestartCount(), 0);
    QCOMPARE(finished.size(), 0);
}

void SessionProcessSupervisorTests::supervisorEndsSessionAfterReplacementShellExits()
{
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy restarted(&supervisor, &SessionSupervisor::SessionProcessSupervisor::shellRestarted);
    QSignalSpy finished(&supervisor, &SessionSupervisor::SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));

    QCOMPARE(::kill(static_cast<pid_t>(supervisor.shellProcessId()), SIGTERM), 0);
    QTRY_COMPARE_WITH_TIMEOUT(restarted.size(), 1, 5'000);
    const qint64 replacementProcessId = supervisor.shellProcessId();
    QVERIFY(replacementProcessId > 1);
    QCOMPARE(::kill(static_cast<pid_t>(replacementProcessId), SIGTERM), 0);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5'000);
    QCOMPARE(restarted.size(), 1);
    QCOMPARE(supervisor.shellRestartCount(), 1);
    QVERIFY(finished.constFirst().at(1).toString().contains(QStringLiteral("shell exited")));
    QVERIFY(!supervisor.isRunning());
    QCOMPARE(supervisor.notificationHostProcessId(), 0);
    QCOMPARE(supervisor.shellProcessId(), 0);
}

void SessionProcessSupervisorTests::supervisorEndsSessionWhenShellRestartCannotStart()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString source = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    const QString disposableShell =
        temporaryDirectory.filePath(QStringLiteral("disposable-shell-helper"));
    QVERIFY(QFile::copy(source, disposableShell));
    QVERIFY(QFile::setPermissions(disposableShell, QFile::permissions(source)));

    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable = source;
    options.shellExecutable = disposableShell;
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy restarted(&supervisor, &SessionSupervisor::SessionProcessSupervisor::shellRestarted);
    QSignalSpy finished(&supervisor, &SessionSupervisor::SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    const qint64 initialShellProcessId = supervisor.shellProcessId();
    QVERIFY(initialShellProcessId > 1);
    QVERIFY(QFile::remove(disposableShell));

    QCOMPARE(::kill(static_cast<pid_t>(initialShellProcessId), SIGTERM), 0);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5'000);
    QCOMPARE(restarted.size(), 0);
    QCOMPARE(supervisor.shellRestartCount(), 1);
    QVERIFY(
        finished.constFirst().at(1).toString().contains(QStringLiteral("could not restart shell")));
    QVERIFY(!supervisor.isRunning());
    QCOMPARE(supervisor.notificationHostProcessId(), 0);
    QCOMPARE(supervisor.shellProcessId(), 0);
}

void SessionProcessSupervisorTests::supervisorRollsBackWhenTheSecondChildCannotStart()
{
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral("/definitely/missing/qindaqt-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QString error;
    QVERIFY(!supervisor.start(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!supervisor.isRunning());
}

void SessionProcessSupervisorTests::session1AuthenticatesShellAndStopsChildrenInOrder()
{
    qputenv("QINDAQT_TEST_SESSION1_LOGOUT", QByteArrayLiteral("1"));
    qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", QByteArrayLiteral("30000"));
    const auto environmentGuard = qScopeGuard([] {
        qunsetenv("QINDAQT_TEST_SESSION1_LOGOUT");
        qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
    });
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable =
        QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.networkSecretAgentExecutable =
        QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy finished(&supervisor,
                        &SessionSupervisor::SessionProcessSupervisor::finished);
    QSignalSpy stopping(
        &supervisor,
        &SessionSupervisor::SessionProcessSupervisor::childStopRequested);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    SessionSupervisor::SessionService service(
        supervisor, QDBusConnection::sessionBus());
    QVERIFY2(service.start(&error), qPrintable(error));

    QDBusInterface unauthorized(
        QStringLiteral("org.qindaqt.Session1"),
        QStringLiteral("/org/qindaqt/Session1"),
        QStringLiteral("org.qindaqt.Session1"),
        QDBusConnection::sessionBus());
    QDBusPendingCallWatcher deniedWatcher(
        unauthorized.asyncCall(QStringLiteral("CanLogout")));
    QSignalSpy deniedFinished(&deniedWatcher, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE_WITH_TIMEOUT(deniedFinished.size(), 1, 2'000);
    const QDBusPendingReply<bool> denied = deniedWatcher;
    QVERIFY(!denied.isValid());
    QCOMPARE(denied.error().name(),
             QStringLiteral("org.qindaqt.Session1.Error.Unauthorized"));

    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5'000);
    QCOMPARE(finished.first().at(0).toInt(), 0);
    QCOMPARE(finished.first().at(1).toString(), QStringLiteral("logout requested"));
    QCOMPARE(stopping.size(), 3);
    QCOMPARE(stopping.at(0).at(0).toString(), QStringLiteral("shell"));
    QCOMPARE(stopping.at(1).at(0).toString(), QStringLiteral("notification-host"));
    QCOMPARE(stopping.at(2).at(0).toString(), QStringLiteral("network-secret-agent"));
    QVERIFY(!supervisor.isRunning());
    QVERIFY(!supervisor.canLogout());
}

void SessionProcessSupervisorTests::optionalSecretAgentRestartsOnceWithoutBlockingSession()
{
    qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable =
        QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.networkSecretAgentExecutable =
        QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy restarted(
        &supervisor,
        &SessionSupervisor::SessionProcessSupervisor::networkSecretAgentRestarted);
    QSignalSpy finished(&supervisor,
                        &SessionSupervisor::SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QVERIFY(supervisor.isRunning());
    QTRY_COMPARE_WITH_TIMEOUT(restarted.size(), 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(supervisor.networkSecretAgentProcessId(), qint64(0),
                              5'000);
    QCOMPARE(supervisor.networkSecretAgentRestartCount(), 1);
    QCOMPARE(finished.size(), 0);
    QVERIFY(supervisor.isRunning());
    supervisor.stop();
}

void SessionProcessSupervisorTests::optionalWelcomeStartsAfterShellAndDoesNotRestart()
{
    qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", QByteArrayLiteral("30000"));
    const auto environmentGuard = qScopeGuard([] {
        qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
    });
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable =
        QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.networkSecretAgentExecutable.clear();
    options.welcomeExecutable = QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42'424;
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy finished(&supervisor,
                        &SessionSupervisor::SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QVERIFY(supervisor.shellProcessId() > 1);
    QTRY_VERIFY_WITH_TIMEOUT(supervisor.welcomeProcessId() > 1, 5'000);

    const qint64 welcomeProcessId = supervisor.welcomeProcessId();
    QFile argumentsFile(QStringLiteral("/proc/%1/cmdline").arg(welcomeProcessId));
    QVERIFY(argumentsFile.open(QIODevice::ReadOnly));
    const QByteArray arguments = argumentsFile.readAll();
    QVERIFY(arguments.contains("--first-launch"));

    QCOMPARE(::kill(static_cast<pid_t>(welcomeProcessId), SIGTERM), 0);
    QTRY_COMPARE_WITH_TIMEOUT(supervisor.welcomeProcessId(), qint64(0), 5'000);
    QTest::qWait(100);
    QCOMPARE(supervisor.welcomeProcessId(), qint64(0));
    QVERIFY(supervisor.isRunning());
    QCOMPARE(finished.size(), 0);
    supervisor.stop();
}

void SessionProcessSupervisorTests::session1DescriptorMatchesTheFixedSurface()
{
    QFile descriptor(QStringLiteral(QINDAQT_SESSION1_XML));
    QVERIFY(descriptor.open(QIODevice::ReadOnly));
    const QByteArray xml = descriptor.readAll();
    QVERIFY(xml.contains("interface name=\"org.qindaqt.Session1\""));
    QCOMPARE(xml.count("method name=\"CanLogout\""), 1);
    QCOMPARE(xml.count("method name=\"Logout\""), 1);
    QCOMPARE(xml.count("direction=\"out\""), 1);
}

QTEST_GUILESS_MAIN(SessionProcessSupervisorTests)

#include "tst_session_process_supervisor.moc"

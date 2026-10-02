// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <cerrno>
#include <csignal>

class NativePowerStartupTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void missingNativePrerequisites_data() {
        QTest::addColumn<bool>("exclusive");
        QTest::newRow("exclusive-refused") << true;
        QTest::newRow("off-keeps-compatible-power-child") << false;
    }
    void missingNativePrerequisites() {
        QFETCH(bool, exclusive);
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString marker = home.filePath(QStringLiteral("power-started"));
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), home.path());
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), home.filePath("config"));
        environment.insert(QStringLiteral("XDG_DATA_HOME"), home.filePath("data"));
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), home.filePath("cache"));
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), home.path());
        environment.insert(QStringLiteral("PATH"), home.filePath("empty-path"));
        environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent"));
        environment.insert(QStringLiteral("QINDAQT_TEST_POWER_START_MARKER"), marker);
        environment.remove(QStringLiteral("DISPLAY"));
        environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
        environment.remove(QStringLiteral("QINDAQT_TEST_SESSION1_LOGOUT"));
        QProcess process;
        process.setProcessEnvironment(environment);
        const auto cleanup = qScopeGuard([&] {
            if (process.state() != QProcess::NotRunning) {
                process.kill(); process.waitForFinished(3000);
            }
        });
        process.start(QStringLiteral(QINDAQT_SESSION_EXECUTABLE), {
            "--notification-host", QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER),
            "--shell", QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER),
            "--profile", "test-hold-shell", "--native-power", exclusive ? "exclusive" : "off",
            "--powerdevil", QStringLiteral(QINDAQT_SESSION_POWER_START_PROBE),
            "--network-secret-agent", "", "--welcome", "", "--desktop-controls", "",
            "--night-light", "", "--no-removable-media", "--no-polkit-agent",
            "--no-global-shortcut-daemon", "--no-input-method-daemon", "--no-autostart",
            "--no-portal", "--no-keyring"
        });
        QVERIFY(process.waitForStarted(3000));
        if (exclusive) {
            QVERIFY(process.waitForFinished(10000));
            QCOMPARE(process.exitStatus(), QProcess::NormalExit);
            QCOMPARE(process.exitCode(), 2);
            QVERIFY(process.readAllStandardError().contains("compositor owner is unavailable"));
            QVERIFY(!QFile::exists(marker));
        } else {
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 5000);
            QVERIFY(process.state() != QProcess::NotRunning);
            QFile file(marker); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto child = file.readAll().toLongLong(); QVERIFY(child > 1);
            QCOMPARE(::kill(static_cast<pid_t>(child), 0), 0);
            auto request = QDBusMessage::createMethodCall("org.qindaqt.Session1", "/org/qindaqt/Session1",
                                                        "org.qindaqt.Session1", "Logout");
            auto reply = QDBusConnection::sessionBus().asyncCall(request, 2000);
            QTRY_VERIFY_WITH_TIMEOUT(reply.isFinished(), 3000);
            QCOMPARE(reply.reply().type(), QDBusMessage::ReplyMessage);
            QTRY_COMPARE_WITH_TIMEOUT(process.state(), QProcess::NotRunning, 5000);
            QCOMPARE(::kill(static_cast<pid_t>(child), 0), -1); QCOMPARE(errno, ESRCH);
        }
    }
};
QTEST_GUILESS_MAIN(NativePowerStartupTest)
#include "tst_native_power_startup.moc"

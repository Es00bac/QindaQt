// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_service/resident_settings_service.h>
#include <qindaqt/settings/settings_schema.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

class ObsLoginHelperTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void confirmedTrueFalseExternalChangeAndNoOwner();
};

void ObsLoginHelperTest::confirmedTrueFalseExternalChangeAndNoOwner() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString bin = root.filePath(QStringLiteral("bin"));
    QVERIFY(QDir().mkpath(bin));
    const QString marker = root.filePath(QStringLiteral("obs-runs"));
    QFile fakeObs(QDir(bin).filePath(QStringLiteral("obs")));
    QVERIFY(fakeObs.open(QIODevice::WriteOnly));
    const QByteArray script = "#!/bin/sh\nprintf run\\n >> " + marker.toUtf8() + "\n";
    QVERIFY(fakeObs.write(script) == script.size());
    fakeObs.close();
    QVERIFY(fakeObs.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner));

    // Custom bus config has no servicedir: a missing private Settings1 owner
    // can never activate a host-installed provider or ambient user state.
    QFile busConfig(root.filePath(QStringLiteral("private-bus.conf")));
    QVERIFY(busConfig.open(QIODevice::WriteOnly));
    QVERIFY(busConfig.write("<busconfig><type>session</type>"
                            "<listen>unix:tmpdir=/tmp</listen><auth>EXTERNAL</auth>"
                            "<policy context=\"default\"><allow send_destination=\"*\"/>"
                            "<allow eavesdrop=\"true\"/><allow own=\"*\"/>"
                            "</policy></busconfig>") > 0);
    busConfig.close();
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--config-file=") + busConfig.fileName(),
                  QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    auto serviceBus = QDBusConnection::connectToBus(address, QStringLiteral("obs-login-service"));
    auto clientBus = QDBusConnection::connectToBus(address, QStringLiteral("obs-login-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), address);
    environment.insert(QStringLiteral("PATH"), bin);
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), root.filePath(QStringLiteral("config")));
    environment.insert(QStringLiteral("XDG_CONFIG_DIRS"), root.filePath(QStringLiteral("system")));
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), root.filePath(QStringLiteral("runtime")));
    environment.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("QindaQt"));
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("runtime"))));
    const auto launch = [&environment] {
        auto process = std::make_unique<QProcess>();
        process->setProcessEnvironment(environment);
        process->start(QStringLiteral(QINDAQT_OBS_LOGIN_EXECUTABLE));
        return process;
    };
    const auto markerCount = [&marker] {
        QFile file(marker);
        if (!file.open(QIODevice::ReadOnly)) return 0;
        return int(file.readAll().count("run"));
    };

    QString error;
    auto schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(schema && legacy, qPrintable(error));
    ResidentSettingsService service(
        serviceBus, *schema, *legacy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"),
        root.filePath(QStringLiteral("user.json")));
    QVERIFY(service.start().ok());
    QtSettingsTransport transport(clientBus);
    SettingsClient client(transport, {QStringLiteral("services.obsStartAtLogin")});
    QVERIFY(client.start());
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready, 2'000);
    QVERIFY(!client.snapshot()->values.value(QStringLiteral("services.obsStartAtLogin")).toBool());

    // Fresh login with persisted false exits without starting even the fake OBS.
    auto disabled = launch();
    QVERIFY(disabled->waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(disabled->state() == QProcess::NotRunning, 7'000);
    QCOMPARE(disabled->exitCode(), 0);
    QCOMPARE(markerCount(), 0);

    // External Settings1 edit, with no Settings route open, is consumed at
    // the next login by the same stable entry/helper path.
    QVERIFY(client.setUserValue(QStringLiteral("services.obsStartAtLogin"), true));
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready
                              && !client.writeInFlight()
                              && client.snapshot()->values.value(
                                     QStringLiteral("services.obsStartAtLogin")).toBool(), 3'000);
    auto enabled = launch();
    QVERIFY(enabled->waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(enabled->state() == QProcess::NotRunning, 7'000);
    QCOMPARE(enabled->exitCode(), 0);
    QCOMPARE(markerCount(), 1);

    QVERIFY(client.setUserValue(QStringLiteral("services.obsStartAtLogin"), false));
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready
                              && !client.writeInFlight()
                              && !client.snapshot()->values.value(
                                      QStringLiteral("services.obsStartAtLogin")).toBool(), 3'000);
    auto disabledAgain = launch();
    QVERIFY(disabledAgain->waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(disabledAgain->state() == QProcess::NotRunning, 7'000);
    QCOMPARE(markerCount(), 1);

    service.stop();
    client.stop();
    QTRY_VERIFY_WITH_TIMEOUT(serviceBus.interface()->serviceOwner(
                                  QStringLiteral("org.qindaqt.Settings1")).value().isEmpty(), 2'000);
    // No owner: a bounded wait exits without launch. Killing a waiting
    // helper also never turns into a delayed OBS start.
    auto canceled = launch();
    QVERIFY(canceled->waitForStarted());
    canceled->kill();
    QVERIFY(canceled->waitForFinished());
    QCOMPARE(markerCount(), 1);
    QElapsedTimer noOwnerWait;
    noOwnerWait.start();
    auto noOwner = launch();
    QVERIFY(noOwner->waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(noOwner->state() == QProcess::NotRunning, 7'000);
    QCOMPARE(noOwner->exitCode(), 0);
    QVERIFY2(noOwnerWait.elapsed() >= 4'000,
             qPrintable(QStringLiteral("no-owner gate exited after %1 ms: %2")
                            .arg(noOwnerWait.elapsed())
                            .arg(QString::fromUtf8(noOwner->readAllStandardError()))));
    QCOMPARE(markerCount(), 1);

    daemon.kill();
    QVERIFY(daemon.waitForFinished());
    QDBusConnection::disconnectFromBus(QStringLiteral("obs-login-service"));
    QDBusConnection::disconnectFromBus(QStringLiteral("obs-login-client"));
}

QTEST_GUILESS_MAIN(ObsLoginHelperTest)
#include "tst_obs_login_helper.moc"

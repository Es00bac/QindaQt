// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellstartuppreferences.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

class ShellStartupPreferencesTests final : public QObject {
    Q_OBJECT

private slots:
    void savedPreferencesSurviveIntoAFreshStartupRead();
    void absentServiceFallsBackWithinDeadline();
};

void ShellStartupPreferencesTests::savedPreferencesSurviveIntoAFreshStartupRead()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceConnection = QStringLiteral("startup-service-") + suffix;
    const QString writerConnection = QStringLiteral("startup-writer-") + suffix;
    const QString readerConnection = QStringLiteral("startup-reader-") + suffix;
    auto serviceBus = QDBusConnection::connectToBus(address, serviceConnection);
    auto writerBus = QDBusConnection::connectToBus(address, writerConnection);
    auto readerBus = QDBusConnection::connectToBus(address, readerConnection);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(writerBus.isConnected());
    QVERIFY(readerBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"),
        nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"),
        nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ResidentSettingsService service(
        serviceBus, *active, *legacy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"),
        directory.filePath(QStringLiteral("user.json")));
    QVERIFY(service.start().ok());

    // The Customize route's save path: one ordinary client commits the
    // selection, then every consumer object is destroyed (process exit).
    {
        QtSettingsTransport transport(writerBus);
        SettingsClient writer(transport, ShellPreferenceValues::scopedKeys(),
                              {.requestTimeoutMilliseconds = 500,
                               .debounceMilliseconds = 0,
                               .retryMilliseconds = {10, 20}});
        QVERIFY2(writer.start(&error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(writer.state() == ClientState::Ready, 2'000);
        const QList<QPair<QString, QVariant>> writes{
            {QStringLiteral("panels.layoutProfile"), QStringLiteral("mate-inspired")},
            {QStringLiteral("appearance.theme"), QStringLiteral("qinda-light")},
            {QStringLiteral("appearance.colorScheme"), QStringLiteral("light")},
            {QStringLiteral("accessibility.highContrast"), true},
            {QStringLiteral("accessibility.reducedTransparency"), true},
            {QStringLiteral("accessibility.textScale"), 1.5},
        };
        for (const auto &[key, value] : writes) {
            QVERIFY2(writer.setUserValue(key, value, &error), qPrintable(error));
            QTRY_VERIFY_WITH_TIMEOUT(!writer.writeInFlight(), 2'000);
            QTRY_VERIFY_WITH_TIMEOUT(
                writer.snapshot()->values.value(key) == value, 2'000);
        }
    }

    // Ordinary startup: a fresh reader with no command-line profile/theme
    // arguments observes the confirmed selection through Settings1 only.
    const auto values = readConfirmedShellPreferences(readerBus, 2'000, &error);
    QVERIFY2(values.has_value(), qPrintable(error));
    QCOMPARE(values->layoutProfileId, QStringLiteral("mate-inspired"));
    QCOMPARE(values->themeId, QStringLiteral("qinda-light"));
    QVERIFY(values->accessibility.highContrast);
    QVERIFY(values->accessibility.reducedTransparency);
    QCOMPARE(values->accessibility.textScale, 1.5);
    // Schema defaults fill the untouched keys.
    QVERIFY(!values->accessibility.reducedMotion);
    QCOMPARE(resolveStartupProfileId({}, values), QStringLiteral("mate-inspired"));
    QCOMPARE(resolveStartupProfileId(QStringLiteral("unity-inspired"), values),
             QStringLiteral("unity-inspired"));

    service.stop();
    QDBusConnection::disconnectFromBus(serviceConnection);
    QDBusConnection::disconnectFromBus(writerConnection);
    QDBusConnection::disconnectFromBus(readerConnection);
    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

void ShellStartupPreferencesTests::absentServiceFallsBackWithinDeadline()
{
    QTemporaryDir busDirectory(QStringLiteral("/tmp/qindaqt-bus-XXXXXX"));
    QVERIFY(busDirectory.isValid());
    QFile config(busDirectory.filePath(QStringLiteral("dbus.conf")));
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
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
    QVERIFY(config.write(configContents) == configContents.size());
    config.close();

    QProcess daemon;
    const auto connection = QStringLiteral("startup-orphan-")
        + QString::number(QCoreApplication::applicationPid());
    const auto cleanup = qScopeGuard([&] {
        QDBusConnection::disconnectFromBus(connection);
        if (daemon.state() != QProcess::NotRunning) {
            daemon.kill();
        }
        daemon.waitForFinished();
    });
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--config-file"), config.fileName(),
                  QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    const bool addressReady = daemon.waitForReadyRead();
    if (!addressReady) {
        qWarning().noquote() << daemon.readAllStandardError();
    }
    QVERIFY(addressReady);
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    auto bus = QDBusConnection::connectToBus(address, connection);
    QVERIFY(bus.isConnected());

    QString error;
    QElapsedTimer elapsed;
    elapsed.start();
    const auto values = readConfirmedShellPreferences(bus, 300, &error);
    // No Settings1 owner: startup must fall back promptly, never hang.
    QVERIFY(!values.has_value());
    QVERIFY(!error.isEmpty());
    QVERIFY(elapsed.elapsed() < 5'000);
}

QTEST_GUILESS_MAIN(ShellStartupPreferencesTests)
#include "tst_shellstartuppreferences.moc"

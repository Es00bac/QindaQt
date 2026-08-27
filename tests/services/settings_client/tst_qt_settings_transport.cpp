// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

class QtSettingsTransportTests final : public QObject {
    Q_OBJECT
private slots:
    void exactOwnerCommitReplacementAndLocalBusLoss();
};

void QtSettingsTransportTests::exactOwnerCommitReplacementAndLocalBusLoss()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    auto serviceBus = QDBusConnection::connectToBus(address, QStringLiteral("settings-service-") + suffix);
    auto clientBus = QDBusConnection::connectToBus(address, QStringLiteral("settings-client-") + suffix);
    auto replacementBus = QDBusConnection::connectToBus(address, QStringLiteral("settings-replacement-") + suffix);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    QVERIFY(replacementBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir directory;
    const QString storage = directory.filePath(QStringLiteral("user.json"));
    const QString profileDefaults = QStringLiteral(
        QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json");
    ResidentSettingsService first(serviceBus, *active, *legacy, profileDefaults, storage);
    QVERIFY(first.start().ok());

    QtSettingsTransport transport(clientBus);
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 500, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10, 20}});
    QSignalSpy uncertain(&client, &SettingsClient::commitUncertain);
    QVERIFY2(client.start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready, 2'000);
    QCOMPARE(client.snapshot()->values.value(QStringLiteral("services.doNotDisturb")).toBool(), false);

    QtSettingsTransport profileTransport(clientBus);
    SettingsClient profileClient(profileTransport,
                                 {QStringLiteral("appearance.animationDurationMs")},
                                 {.requestTimeoutMilliseconds = 500,
                                  .debounceMilliseconds = 0,
                                  .retryMilliseconds = {10, 20}});
    QVERIFY(profileClient.start(&error));
    QTRY_VERIFY_WITH_TIMEOUT(profileClient.state() == ClientState::Ready, 2'000);
    QCOMPARE(profileClient.snapshot()->values
                 .value(QStringLiteral("appearance.animationDurationMs")).toInt(), 160);
    QCOMPARE(profileClient.snapshot()->sourceLayers
                 .value(QStringLiteral("appearance.animationDurationMs")).toString(),
             QStringLiteral("profile-defaults"));
    QVERIFY(profileClient.setUserValue(QStringLiteral("appearance.animationDurationMs"),
                                       240, &error));
    QTRY_VERIFY_WITH_TIMEOUT(profileClient.state() == ClientState::Ready
                                 && !profileClient.writeInFlight()
                                 && profileClient.snapshot()->values
                                        .value(QStringLiteral("appearance.animationDurationMs"))
                                        .toInt() == 240,
                             2'000);
    QCOMPARE(profileClient.snapshot()->sourceLayers
                 .value(QStringLiteral("appearance.animationDurationMs")).toString(),
             QStringLiteral("user-overrides"));
    QVERIFY(profileClient.removeUserValue(QStringLiteral("appearance.animationDurationMs"),
                                          &error));
    QTRY_VERIFY_WITH_TIMEOUT(profileClient.state() == ClientState::Ready
                                 && !profileClient.writeInFlight()
                                 && profileClient.snapshot()->values
                                        .value(QStringLiteral("appearance.animationDurationMs"))
                                        .toInt() == 160,
                             2'000);
    QCOMPARE(profileClient.snapshot()->sourceLayers
                 .value(QStringLiteral("appearance.animationDurationMs")).toString(),
             QStringLiteral("profile-defaults"));

    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready && client.snapshot()
                                 && client.snapshot()->revision == first.revision(),
                             2'000);
    QVERIFY(client.setUserValue(QStringLiteral("services.doNotDisturb"), true, &error));
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready && !client.writeInFlight()
                                 && client.snapshot()->values
                                        .value(QStringLiteral("services.doNotDisturb")).toBool(),
                             2'000);

    QtSettingsTransport objectTransport(clientBus);
    SettingsClient objectClient(objectTransport, {QStringLiteral("displays.configuration")},
                                {.requestTimeoutMilliseconds = 500,
                                 .debounceMilliseconds = 0,
                                 .retryMilliseconds = {10, 20}});
    QVERIFY(objectClient.start(&error));
    QTRY_VERIFY_WITH_TIMEOUT(objectClient.state() == ClientState::Ready, 2'000);
    const QVariantMap nested{
        {QStringLiteral("outputs"), QVariantList{
             QVariantMap{{QStringLiteral("name"), QStringLiteral("Virtual-1")},
                         {QStringLiteral("position"), QVariantList{0, 0}},
                         {QStringLiteral("modes"), QVariantList{
                              QVariantMap{{QStringLiteral("width"), 1920},
                                          {QStringLiteral("height"), 1080}}}}}}}};
    QVERIFY2(objectClient.setUserValue(QStringLiteral("displays.configuration"), nested, &error),
             qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(objectClient.state() == ClientState::Ready
                                 && objectClient.snapshot()->values
                                        .value(QStringLiteral("displays.configuration")).toMap()
                                    == nested,
                             2'000);

    const QString firstOwner = client.snapshot()->owner;
    const QString firstEpoch = client.snapshot()->epoch;
    first.stop();
    ResidentSettingsService replacement(replacementBus, *active, *legacy,
                                        profileDefaults, storage);
    QVERIFY(replacement.start().ok());
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready
                                 && client.snapshot()->owner != firstOwner,
                             2'000);
    QVERIFY(client.snapshot()->epoch != firstEpoch);
    QCOMPARE(client.snapshot()->values.value(QStringLiteral("services.doNotDisturb")).toBool(), true);
    QCOMPARE(uncertain.size(), 0);

    // Exercise the public reusable lifecycle on one SettingsClient/transport
    // pair rather than relying on composition-root object reconstruction.
    client.stop();
    QCOMPARE(client.state(), ClientState::Unavailable);
    QVERIFY2(client.start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready, 2'000);
    QCOMPARE(client.snapshot()->values.value(QStringLiteral("services.doNotDisturb")).toBool(), true);

    daemon.kill();
    QVERIFY(daemon.waitForFinished());
    QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Unavailable, 2'000);
    QCOMPARE(client.snapshot()->values.value(QStringLiteral("services.doNotDisturb")).toBool(), true);

    client.stop();
    profileClient.stop();
    objectClient.stop();
    replacement.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("settings-service-") + suffix);
    QDBusConnection::disconnectFromBus(QStringLiteral("settings-client-") + suffix);
    QDBusConnection::disconnectFromBus(QStringLiteral("settings-replacement-") + suffix);
}

QTEST_GUILESS_MAIN(QtSettingsTransportTests)
#include "tst_qt_settings_transport.moc"

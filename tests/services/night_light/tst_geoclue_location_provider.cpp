// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/night_light/automatic_location_provider.h>

#include <QDBusContext>
#include <QDBusConnectionInterface>
#include <QDBusObjectPath>
#include <QDBusConnection>
#include <QProcess>
#include <QTimer>
#include <QtTest>

using namespace QindaQt::Services::NightLight;

namespace {
class PrivateBus final {
public:
    PrivateBus()
    {
        process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
            {QStringLiteral("--session"), QStringLiteral("--nofork"),
             QStringLiteral("--print-address=1"), QStringLiteral("--print-pid=1")});
        if (!process.waitForStarted() || !process.waitForReadyRead()) return;
        address = QString::fromUtf8(process.readLine()).trimmed();
        process.readLine();
    }
    ~PrivateBus() { process.terminate(); process.waitForFinished(); }
    QProcess process;
    QString address;
};

class FakeLocation final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Location")
    Q_PROPERTY(double Latitude READ latitude CONSTANT)
    Q_PROPERTY(double Longitude READ longitude CONSTANT)
    Q_PROPERTY(qulonglong Timestamp READ timestamp CONSTANT)
public:
    double latitude() const { return 40.01; }
    double longitude() const { return -105.27; }
    qulonglong timestamp() const { return qulonglong(QDateTime::currentSecsSinceEpoch()); }
};

class FakeMissingLatitude final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Location")
    Q_PROPERTY(double Longitude READ longitude CONSTANT)
    Q_PROPERTY(qulonglong Timestamp READ timestamp CONSTANT)
public:
    double longitude() const { return -105.27; }
    qulonglong timestamp() const { return qulonglong(QDateTime::currentSecsSinceEpoch()); }
};

class FakeInvalidLatitude final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Location")
    Q_PROPERTY(QString Latitude READ latitude CONSTANT)
    Q_PROPERTY(double Longitude READ longitude CONSTANT)
    Q_PROPERTY(qulonglong Timestamp READ timestamp CONSTANT)
public:
    QString latitude() const { return QStringLiteral("not-a-coordinate"); }
    double longitude() const { return -105.27; }
    qulonglong timestamp() const { return qulonglong(QDateTime::currentSecsSinceEpoch()); }
};

class FakeClient final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Client")
    Q_PROPERTY(QString DesktopId READ desktopId WRITE setDesktopId)
    Q_PROPERTY(quint32 RequestedAccuracyLevel READ requestedAccuracy WRITE setRequestedAccuracy)
public:
    FakeClient() = default;
    QString desktopId() const { return m_desktopId; }
    void setDesktopId(const QString &value) { m_desktopId = value; }
    quint32 requestedAccuracy() const { return m_accuracy; }
    int stopCalls = 0;
    void setRequestedAccuracy(quint32 value) { m_accuracy = value; }
public Q_SLOTS:
    void Start() { Q_EMIT LocationUpdated(QDBusObjectPath(QStringLiteral("/org/freedesktop/GeoClue2/Location/old")), QDBusObjectPath(QStringLiteral("/org/freedesktop/GeoClue2/Location/current"))); }
    void Stop() { ++stopCalls; }
Q_SIGNALS:
    void LocationUpdated(const QDBusObjectPath &oldPath, const QDBusObjectPath &newPath);
private:
    QString m_desktopId;
    quint32 m_accuracy = 0;
};

class FakeManager final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Manager")
public:
    explicit FakeManager(QDBusConnection bus, bool deny = false, bool delay = false)
        : m_bus(std::move(bus)), m_deny(deny), m_delay(delay) {}
public Q_SLOTS:
    QDBusObjectPath CreateClient()
    {
        ++calls;
        if (m_deny) {
            sendErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                           QStringLiteral("synthetic permission denial"));
            return {};
        }
        if (m_delay) {
            const QDBusMessage request = message();
            setDelayedReply(true);
            QTimer::singleShot(120, this, [this, request] {
                m_bus.send(request.createReply(
                    {QVariant::fromValue(QDBusObjectPath(
                        QStringLiteral("/org/freedesktop/GeoClue2/Client/1")))}));
            });
            return {};
        }
        return QDBusObjectPath(QStringLiteral("/org/freedesktop/GeoClue2/Client/1"));
    }
public:
    int calls = 0;
private:
    QDBusConnection m_bus;
    bool m_deny;
    bool m_delay;
};
}

class GeoClueLocationProviderTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void requestsWithStableDesktopIdentityAndCityAccuracy();
    void denialAndStopClearLocation();
    void lateCreateReplyCannotRestoreStoppedRequest();
    void ownerReplacementClearsOldFixAndUsesNewOwner();
    void missingCoordinateIsUnavailable();
    void mistypedCoordinateIsUnavailable();
};

void GeoClueLocationProviderTests::requestsWithStableDesktopIdentityAndCityAccuracy()
{
    PrivateBus daemon;
    QVERIFY(!daemon.address.isEmpty());
    auto serviceBus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-provider-server"));
    auto bus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-provider-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(bus.isConnected());
    FakeLocation location;
    FakeClient client;
    FakeManager manager(serviceBus);
    QVERIFY(serviceBus.interface()->registerService(QStringLiteral("org.freedesktop.GeoClue2"))
            == QDBusConnectionInterface::ServiceRegistered);
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"),
                               &manager, QDBusConnection::ExportAllSlots));
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Client/1"),
                               &client, QDBusConnection::ExportAllSlots
                                   | QDBusConnection::ExportAllProperties
                                   | QDBusConnection::ExportAllSignals));
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Location/current"),
                               &location, QDBusConnection::ExportAllProperties));
    GeoClueLocationProvider provider(bus);
    QSignalSpy changes(&provider, &AutomaticLocationProvider::changed);
    provider.request();
    QTRY_VERIFY2(provider.state() == AutomaticLocationState::Available, qPrintable(provider.diagnostic()));
    QCOMPARE(manager.calls, 1);
    QCOMPARE(client.desktopId(), QStringLiteral("org.qindaqt.NightLight"));
    QCOMPARE(client.requestedAccuracy(), quint32(4));
    QVERIFY(provider.fix());
    QCOMPARE(provider.fix()->latitudeDegrees, 40.01);
    QCOMPARE(provider.fix()->longitudeDegrees, -105.27);
    QVERIFY(changes.size() >= 2);
    provider.stop();
    QTRY_COMPARE(client.stopCalls, 1);
    QVERIFY(!provider.fix());
    QCOMPARE(provider.state(), AutomaticLocationState::Stopped);
    bus.unregisterObject(QStringLiteral("/org/freedesktop/GeoClue2/Location/current"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/GeoClue2/Client/1"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"));
    bus.unregisterService(QStringLiteral("org.freedesktop.GeoClue2"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-provider-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-provider-server"));
}

void GeoClueLocationProviderTests::denialAndStopClearLocation()
{
    PrivateBus daemon;
    QVERIFY(!daemon.address.isEmpty());
    auto serviceBus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-denial-server"));
    auto bus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-denial-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(bus.isConnected());
    FakeClient client;
    FakeManager manager(serviceBus, true);
    QVERIFY(serviceBus.interface()->registerService(QStringLiteral("org.freedesktop.GeoClue2"))
            == QDBusConnectionInterface::ServiceRegistered);
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"),
                               &manager, QDBusConnection::ExportAllSlots));
    GeoClueLocationProvider provider(bus);
    provider.request();
    QTRY_COMPARE(provider.state(), AutomaticLocationState::Denied);
    QVERIFY(provider.diagnostic().contains(QStringLiteral("permission")));
    QVERIFY(!provider.fix());
    provider.stop();
    QCOMPARE(provider.state(), AutomaticLocationState::Stopped);
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-denial-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-denial-server"));
}

void GeoClueLocationProviderTests::lateCreateReplyCannotRestoreStoppedRequest()
{
    PrivateBus daemon;
    QVERIFY(!daemon.address.isEmpty());
    auto serviceBus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-stale-server"));
    auto bus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-stale-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(bus.isConnected());
    FakeClient client;
    FakeManager manager(serviceBus, false, true);
    QVERIFY(serviceBus.interface()->registerService(QStringLiteral("org.freedesktop.GeoClue2"))
            == QDBusConnectionInterface::ServiceRegistered);
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"),
                               &manager, QDBusConnection::ExportAllSlots));
    GeoClueLocationProvider provider(bus);
    provider.request();
    QTRY_COMPARE(manager.calls, 1);
    provider.stop();
    QTest::qWait(180);
    QCOMPARE(provider.state(), AutomaticLocationState::Stopped);
    QVERIFY(!provider.fix());
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-stale-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-stale-server"));
}


void GeoClueLocationProviderTests::ownerReplacementClearsOldFixAndUsesNewOwner()
{
    PrivateBus daemon;
    QVERIFY(!daemon.address.isEmpty());
    auto firstService = QDBusConnection::connectToBus(
        daemon.address, QStringLiteral("geoclue-owner-first"));
    auto secondService = QDBusConnection::connectToBus(
        daemon.address, QStringLiteral("geoclue-owner-second"));
    auto clientBus = QDBusConnection::connectToBus(
        daemon.address, QStringLiteral("geoclue-owner-client"));
    QVERIFY(firstService.isConnected());
    QVERIFY(secondService.isConnected());
    QVERIFY(clientBus.isConnected());
    FakeLocation location;
    FakeClient firstClient;
    FakeManager firstManager(firstService);
    QVERIFY(firstService.interface()->registerService(QStringLiteral("org.freedesktop.GeoClue2"))
            == QDBusConnectionInterface::ServiceRegistered);
    QVERIFY(firstService.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"),
                                        &firstManager, QDBusConnection::ExportAllSlots));
    QVERIFY(firstService.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Client/1"),
                                        &firstClient, QDBusConnection::ExportAllSlots
                                            | QDBusConnection::ExportAllProperties
                                            | QDBusConnection::ExportAllSignals));
    QVERIFY(firstService.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Location/current"),
                                        &location, QDBusConnection::ExportAllProperties));
    GeoClueLocationProvider provider(clientBus);
    provider.request();
    QTRY_COMPARE(provider.state(), AutomaticLocationState::Available);
    QVERIFY(provider.fix());

    FakeClient secondClient;
    FakeManager secondManager(secondService, true);
    QVERIFY(firstService.unregisterService(QStringLiteral("org.freedesktop.GeoClue2")));
    QVERIFY(secondService.interface()->registerService(QStringLiteral("org.freedesktop.GeoClue2"))
            == QDBusConnectionInterface::ServiceRegistered);
    QVERIFY(secondService.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"),
                                         &secondManager, QDBusConnection::ExportAllSlots));
    QTRY_COMPARE(provider.state(), AutomaticLocationState::Denied);
    QCOMPARE(secondManager.calls, 1);
    QVERIFY(!provider.fix());
    QVERIFY(provider.diagnostic().contains(QStringLiteral("permission")));
    provider.stop();

    secondService.unregisterObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"));
    firstService.unregisterObject(QStringLiteral("/org/freedesktop/GeoClue2/Location/current"));
    firstService.unregisterObject(QStringLiteral("/org/freedesktop/GeoClue2/Client/1"));
    firstService.unregisterObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-owner-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-owner-first"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-owner-second"));
}


void GeoClueLocationProviderTests::missingCoordinateIsUnavailable()
{
    PrivateBus daemon;
    QVERIFY(!daemon.address.isEmpty());
    auto serviceBus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-missing-server"));
    auto bus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-missing-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(bus.isConnected());
    FakeMissingLatitude location;
    FakeClient client;
    FakeManager manager(serviceBus);
    QVERIFY(serviceBus.interface()->registerService(QStringLiteral("org.freedesktop.GeoClue2"))
            == QDBusConnectionInterface::ServiceRegistered);
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"),
                                      &manager, QDBusConnection::ExportAllSlots));
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Client/1"),
                                      &client, QDBusConnection::ExportAllSlots
                                          | QDBusConnection::ExportAllProperties
                                          | QDBusConnection::ExportAllSignals));
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Location/current"),
                                      &location, QDBusConnection::ExportAllProperties));
    GeoClueLocationProvider provider(bus);
    provider.request();
    QTRY_COMPARE(provider.state(), AutomaticLocationState::Unavailable);
    QVERIFY(!provider.fix());
    QVERIFY(provider.diagnostic().contains(QStringLiteral("coordinates")));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-missing-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-missing-server"));
}

void GeoClueLocationProviderTests::mistypedCoordinateIsUnavailable()
{
    PrivateBus daemon;
    QVERIFY(!daemon.address.isEmpty());
    auto serviceBus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-mistyped-server"));
    auto bus = QDBusConnection::connectToBus(daemon.address, QStringLiteral("geoclue-mistyped-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(bus.isConnected());
    FakeInvalidLatitude location;
    FakeClient client;
    FakeManager manager(serviceBus);
    QVERIFY(serviceBus.interface()->registerService(QStringLiteral("org.freedesktop.GeoClue2"))
            == QDBusConnectionInterface::ServiceRegistered);
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Manager"),
                                      &manager, QDBusConnection::ExportAllSlots));
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Client/1"),
                                      &client, QDBusConnection::ExportAllSlots
                                          | QDBusConnection::ExportAllProperties
                                          | QDBusConnection::ExportAllSignals));
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/freedesktop/GeoClue2/Location/current"),
                                      &location, QDBusConnection::ExportAllProperties));
    GeoClueLocationProvider provider(bus);
    provider.request();
    QTRY_COMPARE(provider.state(), AutomaticLocationState::Unavailable);
    QVERIFY(!provider.fix());
    QVERIFY(provider.diagnostic().contains(QStringLiteral("coordinates")));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-mistyped-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("geoclue-mistyped-server"));
}

QTEST_GUILESS_MAIN(GeoClueLocationProviderTests)
#include "tst_geoclue_location_provider.moc"

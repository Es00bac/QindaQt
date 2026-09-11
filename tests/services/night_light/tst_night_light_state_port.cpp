// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/night_light/night_light_state_port.h>
#include <qindaqt/services/night_light/night_time_schedule_monitor.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::NightLight;

namespace {

const QString kServiceName = QStringLiteral("org.kde.KWin.NightLight");
const QString kObjectPath = QStringLiteral("/org/kde/KWin/NightLight");
const QString kInterfaceName = QStringLiteral("org.kde.KWin.NightLight");
const QString kPropertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");
const QString kScheduleServiceName = QStringLiteral("org.kde.NightTime");

// AGENT-CONTRACT: This fake mirrors the documented KWin interface exactly:
// property names and D-Bus types (bool/u/t), the short preview calls, and
// connection-scoped inhibit locks released when the caller's name disappears.
// Changing this fake changes what the port tests prove.
class FakeNightLightService final : public QObject, public QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.NightLight")
    Q_PROPERTY(bool available MEMBER m_available)
    Q_PROPERTY(bool enabled MEMBER m_enabled)
    Q_PROPERTY(bool running MEMBER m_running)
    Q_PROPERTY(bool inhibited READ inhibited)
    Q_PROPERTY(uint mode MEMBER m_mode)
    Q_PROPERTY(bool daylight MEMBER m_daylight)
    Q_PROPERTY(uint currentTemperature MEMBER m_currentTemperature)
    Q_PROPERTY(uint targetTemperature MEMBER m_targetTemperature)
    Q_PROPERTY(qulonglong previousTransitionDateTime MEMBER m_previousStart)
    Q_PROPERTY(uint previousTransitionDuration MEMBER m_previousDuration)
    Q_PROPERTY(qulonglong scheduledTransitionDateTime MEMBER m_scheduledStart)
    Q_PROPERTY(uint scheduledTransitionDuration MEMBER m_scheduledDuration)

public:
    explicit FakeNightLightService(const QDBusConnection &connection,
                                   QObject *parent = nullptr)
        : QObject(parent), m_connection(connection)
    {
        nameOwnerWatchConnected =
            m_connection.connect(QStringLiteral("org.freedesktop.DBus"),
                                 QStringLiteral("/org/freedesktop/DBus"),
                                 QStringLiteral("org.freedesktop.DBus"),
                                 QStringLiteral("NameOwnerChanged"), this,
                                 SLOT(handleNameOwnerChanged(QString, QString,
                                                             QString)));
    }

    bool inhibited() const { return !m_inhibitCallers.isEmpty(); }

    void setProperties(const QVariantMap &changes)
    {
        for (auto it = changes.constBegin(); it != changes.constEnd(); ++it) {
            setProperty(it.key().toUtf8().constData(), it.value());
        }
        emitPropertiesChanged(changes);
    }

    void emitPropertiesChanged(const QVariantMap &changed)
    {
        QDBusMessage signal = QDBusMessage::createSignal(
            kObjectPath, kPropertiesInterface,
            QStringLiteral("PropertiesChanged"));
        signal.setArguments({QVariant(kInterfaceName), QVariant(changed),
                             QVariant(QStringList())});
        m_connection.send(signal);
    }

    uint inhibitedCallCount = 0;
    uint releasedCallCount = 0;
    uint lastCookie = 0;
    bool nameOwnerWatchConnected = false;
    QList<uint> previewTemperatures;
    uint stopPreviewCount = 0;

public Q_SLOTS:
    Q_SCRIPTABLE uint inhibit()
    {
        const QString caller = message().service();
        const uint cookie = ++m_nextCookie;
        m_inhibitCallers.insert(cookie, caller);
        lastCookie = cookie;
        ++inhibitedCallCount;
        emitPropertiesChanged(
            {{QStringLiteral("inhibited"), QVariant(inhibited())}});
        return cookie;
    }

    Q_SCRIPTABLE void uninhibit(uint cookie)
    {
        if (m_inhibitCallers.remove(cookie) > 0) {
            ++releasedCallCount;
            emitPropertiesChanged(
                {{QStringLiteral("inhibited"), QVariant(inhibited())}});
        }
    }

    Q_SCRIPTABLE void preview(uint temperature)
    {
        previewTemperatures.append(temperature);
    }

    Q_SCRIPTABLE void stopPreview() { ++stopPreviewCount; }

private Q_SLOTS:
    void handleNameOwnerChanged(const QString &name, const QString &,
                                const QString &newOwner)
    {
        if (newOwner.isEmpty() && m_inhibitCallers.values().contains(name)) {
            releaseCaller(name);
        }
    }

private:
    // AGENT-CONTRACT: The inhibition lock is connection-scoped: it is
    // released automatically when the requesting service disappears
    // (org.kde.KWin.NightLight D-Bus XML). A paused Settings client would
    // lose its pause on disconnect, which is why Settings offers no pause.
    void releaseCaller(const QString &caller)
    {
        QList<uint> released;
        for (auto it = m_inhibitCallers.constBegin();
             it != m_inhibitCallers.constEnd(); ++it) {
            if (it.value() == caller) {
                released.append(it.key());
            }
        }
        for (const uint cookie : released) {
            m_inhibitCallers.remove(cookie);
            ++releasedCallCount;
        }
        if (!released.isEmpty()) {
            emitPropertiesChanged(
                {{QStringLiteral("inhibited"), QVariant(inhibited())}});
        }
    }

    QDBusConnection m_connection;
    QMap<uint, QString> m_inhibitCallers;
    uint m_nextCookie = 0;

    bool m_available = true;
    bool m_enabled = false;
    bool m_running = false;
    uint m_mode = 1;
    bool m_daylight = true;
    uint m_currentTemperature = 6500;
    uint m_targetTemperature = 6500;
    qulonglong m_previousStart = 0;
    uint m_previousDuration = 0;
    qulonglong m_scheduledStart = 0;
    uint m_scheduledDuration = 0;
};

// A GetAll answer with the wrong property type: `available` arrives as a
// string, which the port must reject whole.
class MalformedNightLightService final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.NightLight")
    Q_PROPERTY(QString available MEMBER m_available)

public:
    explicit MalformedNightLightService(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private:
    QString m_available = QStringLiteral("true");
};

} // namespace

class NightLightStatePortTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void missingServiceHostileFramesPreviewAndInhibit();
    void scheduleMonitorFailsClosedUntilDaemonAppears();

private:
    static void cleanupBus(QProcess &daemon)
    {
        daemon.kill();
        QVERIFY(daemon.waitForFinished());
    }
};

void NightLightStatePortTests::
    missingServiceHostileFramesPreviewAndInhibit()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    auto serviceBus = QDBusConnection::connectToBus(
        address, QStringLiteral("night-light-service-") + suffix);
    auto clientBus = QDBusConnection::connectToBus(
        address, QStringLiteral("night-light-client-") + suffix);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());

    QtNightLightStatePort port(clientBus);
    QSignalSpy statusSpy(&port, &NightLightStatePort::statusChanged);
    QSignalSpy degradedSpy(&port, &NightLightStatePort::degraded);

    // Negative control: with no service on the bus the port publishes the
    // explicit unavailable frame and reports degradation. It never invents
    // usable state.
    port.start();
    QTRY_VERIFY_WITH_TIMEOUT(statusSpy.count() >= 1, 5'000);
    QCOMPARE(statusSpy.constFirst().at(0).value<NightLightStatus>().available,
             false);
    QVERIFY(degradedSpy.count() >= 1);
    const qsizetype statusesAfterMissingService = statusSpy.count();

    FakeNightLightService fake(serviceBus);
    QVERIFY(serviceBus.registerObject(kObjectPath, &fake,
                                      QDBusConnection::ExportScriptableSlots
                                          | QDBusConnection::
                                              ExportScriptableProperties));
    QVERIFY(serviceBus.registerService(kServiceName) ==
            QDBusConnectionInterface::ServiceRegistered);
    for (int waited = 0;
         waited < 50 && statusSpy.count() <= statusesAfterMissingService;
         ++waited) {
        QTest::qWait(100);
    }
    QTRY_VERIFY_WITH_TIMEOUT(statusSpy.count() > statusesAfterMissingService,
                             5'000);
    const NightLightStatus healthy =
        statusSpy.last().at(0).value<NightLightStatus>();
    QVERIFY(healthy.available);
    QVERIFY(!healthy.enabled);
    QVERIFY(!healthy.running);
    QVERIFY(!healthy.inhibited);
    QCOMPARE(healthy.mode, Mode::DarkLight);
    QVERIFY(healthy.daylight);
    QCOMPARE(healthy.currentTemperatureKelvin, 6500);
    QCOMPARE(healthy.targetTemperatureKelvin, 6500);
    QVERIFY(!healthy.previousTransition.dateTime.isValid());
    QVERIFY(!healthy.scheduledTransition.dateTime.isValid());

    // Live updates: a PropertiesChanged hint triggers a full re-read and the
    // complete frame is republished.
    fake.setProperties({{QStringLiteral("currentTemperature"), QVariant(3400u)},
                        {QStringLiteral("enabled"), QVariant(true)},
                        {QStringLiteral("running"), QVariant(true)}});
    QTRY_VERIFY_WITH_TIMEOUT(
        statusSpy.last().at(0).value<NightLightStatus>()
                .currentTemperatureKelvin
            == 3400,
        5'000);
    const qsizetype statusesAfterLiveUpdate = statusSpy.count();

    // Out-of-range values are rejected whole: the last complete frame stays
    // published and the degradation is reported.
    fake.setProperties({{QStringLiteral("currentTemperature"),
                         QVariant(20000u)}});
    QTRY_VERIFY_WITH_TIMEOUT(degradedSpy.count() >= 2, 5'000);
    QCOMPARE(statusSpy.count(), statusesAfterLiveUpdate);
    QCOMPARE(statusSpy.last().at(0).value<NightLightStatus>()
                 .currentTemperatureKelvin,
             3400);

    // A mode integer from the outdated D-Bus documentation must never map.
    fake.setProperties({{QStringLiteral("mode"), QVariant(3u)}});
    QTRY_VERIFY_WITH_TIMEOUT(degradedSpy.count() >= 3, 5'000);
    QCOMPARE(statusSpy.count(), statusesAfterLiveUpdate);
    // Recovery: a fresh healthy frame (mode restored, temperature moved)
    // republishes; an identical frame would rightly stay deduplicated.
    fake.setProperties({{QStringLiteral("mode"), QVariant(1u)},
                        {QStringLiteral("currentTemperature"),
                         QVariant(3300u)}});
    QTRY_VERIFY_WITH_TIMEOUT(
        statusSpy.last().at(0).value<NightLightStatus>()
                .currentTemperatureKelvin
            == 3300,
        5'000);
    QCOMPARE(statusSpy.last().at(0).value<NightLightStatus>().mode,
             Mode::DarkLight);

    // Preview is forwarded verbatim; out-of-range previews are refused
    // locally without touching the bus; stopPreview is forwarded.
    port.preview(2700);
    port.preview(999);
    port.stopPreview();
    QTRY_VERIFY_WITH_TIMEOUT(fake.previewTemperatures.size() == 1, 5'000);
    QCOMPARE(fake.previewTemperatures.constFirst(), 2700u);
    QTRY_VERIFY_WITH_TIMEOUT(fake.stopPreviewCount == 1, 5'000);
    QVERIFY(degradedSpy.count() >= 4);

    // Malformed reply: a producer whose GetAll carries a wrong property type
    // must not publish a partial frame. The service-reappear cycle ends in
    // the explicit unavailable frame plus a fresh degradation report.
    serviceBus.unregisterService(kServiceName);
    QTRY_VERIFY_WITH_TIMEOUT(
        !statusSpy.last().at(0).value<NightLightStatus>().available, 5'000);
    serviceBus.unregisterObject(kObjectPath);
    const qsizetype degradedBeforeMalformed = degradedSpy.count();
    MalformedNightLightService malformed;
    QVERIFY(serviceBus.registerObject(kObjectPath, &malformed,
                                      QDBusConnection::
                                          ExportScriptableProperties));
    QVERIFY(serviceBus.registerService(kServiceName) ==
            QDBusConnectionInterface::ServiceRegistered);
    QTRY_VERIFY_WITH_TIMEOUT(
        degradedSpy.count() > degradedBeforeMalformed, 5'000);
    QCOMPARE(statusSpy.last().at(0).value<NightLightStatus>().available,
             false);
    serviceBus.unregisterService(kServiceName);
    serviceBus.unregisterObject(kObjectPath);
    QTRY_VERIFY_WITH_TIMEOUT(
        !statusSpy.last().at(0).value<NightLightStatus>().available, 5'000);

    // The inhibit contract: connection-scoped, released when the caller
    // disconnects. This pins the "no pause from Settings" decision (ADR-0136)
    // until the private KWin proof re-checks it against the real plugin.
    // Calls are asynchronous: peer replies only arrive through the event
    // loop, and the fake records the cookie it handed out. The healthy fake
    // must be re-registered first: the malformed section withdrew it.
    QVERIFY(serviceBus.registerObject(kObjectPath, &fake,
                                      QDBusConnection::ExportScriptableSlots
                                          | QDBusConnection::
                                              ExportScriptableProperties));
    QVERIFY(serviceBus.registerService(kServiceName) ==
            QDBusConnectionInterface::ServiceRegistered);
    QTRY_VERIFY_WITH_TIMEOUT(
        statusSpy.last().at(0).value<NightLightStatus>().available, 5'000);
    QVERIFY(fake.nameOwnerWatchConnected);

    {
        // The inhibiting client lives in its own scope so its handle is
        // really gone when the connection is disconnected below: a dropped
        // connection closes its bus name, which is what releases the lock.
        auto inhibitingBus = QDBusConnection::connectToBus(
            address, QStringLiteral("night-light-inhibitor-") + suffix);
        QVERIFY(inhibitingBus.isConnected());
        QDBusMessage inhibitCall = QDBusMessage::createMethodCall(
            kServiceName, kObjectPath, kInterfaceName,
            QStringLiteral("inhibit"));
        inhibitingBus.asyncCall(inhibitCall);
        QTRY_VERIFY_WITH_TIMEOUT(fake.inhibitedCallCount == 1, 5'000);
        QVERIFY(fake.lastCookie != 0);
        QTRY_VERIFY_WITH_TIMEOUT(
            statusSpy.last().at(0).value<NightLightStatus>().inhibited,
            5'000);
    }

    // A Settings process going away loses the lock with its connection.
    QDBusConnection::disconnectFromBus(QStringLiteral("night-light-inhibitor-")
                                       + suffix);
    QTRY_VERIFY_WITH_TIMEOUT(fake.releasedCallCount == 1, 5'000);
    QCOMPARE(fake.inhibited(), false);
    QTRY_VERIFY_WITH_TIMEOUT(
        !statusSpy.last().at(0).value<NightLightStatus>().inhibited, 5'000);

    port.stop();
    serviceBus.unregisterObject(kObjectPath);
    QDBusConnection::disconnectFromBus(QStringLiteral("night-light-service-")
                                       + suffix);
    QDBusConnection::disconnectFromBus(QStringLiteral("night-light-client-")
                                       + suffix);
    cleanupBus(daemon);
}

void NightLightStatePortTests::scheduleMonitorFailsClosedUntilDaemonAppears()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    auto serviceBus = QDBusConnection::connectToBus(
        address, QStringLiteral("night-monitor-service-") + suffix);
    auto clientBus = QDBusConnection::connectToBus(
        address, QStringLiteral("night-monitor-client-") + suffix);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());

    QtNightTimeScheduleMonitor monitor(clientBus);
    QSignalSpy availabilitySpy(
        &monitor, &NightTimeScheduleMonitor::scheduleAvailabilityChanged);

    // Fail closed: no daemon, no schedule truth. The owner check is an
    // asynchronous call to the bus daemon; nothing here blocks the caller.
    monitor.start();
    QTest::qWait(200);
    QCOMPARE(monitor.scheduleAvailable(), false);

    QVERIFY(serviceBus.registerService(kScheduleServiceName) ==
            QDBusConnectionInterface::ServiceRegistered);
    QTRY_VERIFY_WITH_TIMEOUT(monitor.scheduleAvailable(), 5'000);
    QVERIFY(availabilitySpy.count() >= 1);
    QVERIFY(availabilitySpy.last().at(0).toBool());

    QVERIFY(serviceBus.unregisterService(kScheduleServiceName));
    QTRY_VERIFY_WITH_TIMEOUT(!monitor.scheduleAvailable(), 5'000);

    monitor.stop();
    QDBusConnection::disconnectFromBus(
        QStringLiteral("night-monitor-service-") + suffix);
    QDBusConnection::disconnectFromBus(
        QStringLiteral("night-monitor-client-") + suffix);
    cleanupBus(daemon);
}

QTEST_GUILESS_MAIN(NightLightStatePortTests)
#include "tst_night_light_state_port.moc"

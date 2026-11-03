// SPDX-License-Identifier: GPL-3.0-or-later
// AvahiPublisher against a fake org.freedesktop.Avahi on the private test bus.
// The fake runs on its own thread and connection (the publisher uses blocking
// calls), records every call and can inject a name collision or a daemon
// restart. It never reaches the system bus or the real avahi-daemon.
#include "support/scripted_phone.h"

#include <qindaqt/services/link_service/avahi_publisher.h>

#include <QtCore/QMutex>
#include <QtCore/QThread>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusObjectPath>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
struct Calls {
    QMutex mutex;
    QStringList log;          // "New", "Add:<name>:<port>:<txt;...>", "Commit", "Reset", "Free", "Alt:<name>"
    int collisionsToInject = 0;
    void add(const QString &s) { QMutexLocker l(&mutex); log.append(s); }
    QStringList snapshot() { QMutexLocker l(&mutex); return log; }
};

class FakeGroup : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Avahi.EntryGroup")
public:
    FakeGroup(Calls *c, QDBusConnection bus) : m_calls(c), m_bus(std::move(bus)) {}
public Q_SLOTS:
    void AddService(int, int, uint, const QString &name, const QString &type, const QString &, const QString &,
                    ushort port, const QByteArrayList &txt)
    {
        QStringList t;
        for (const QByteArray &b : txt) t << QString::fromUtf8(b);
        m_calls->add(QStringLiteral("Add:%1:%2:%3:%4").arg(name, type).arg(port).arg(t.join(QLatin1Char(';'))));
    }
    void Commit()
    {
        m_calls->add(QStringLiteral("Commit"));
        int state = 2;
        {
            QMutexLocker l(&m_calls->mutex);
            if (m_calls->collisionsToInject > 0) { --m_calls->collisionsToInject; state = 3; }
        }
        QMetaObject::invokeMethod(this, [this, state] { emitState(state); }, Qt::QueuedConnection);
    }
    void Reset() { m_calls->add(QStringLiteral("Reset")); }
    void Free() { m_calls->add(QStringLiteral("Free")); }
Q_SIGNALS:
    void StateChanged(int state, const QString &error);
private:
    void emitState(int state) { Q_EMIT StateChanged(state, QString()); }
    Calls *m_calls;
    QDBusConnection m_bus;
};

class FakeServer : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Avahi.Server")
public:
    FakeServer(Calls *c, FakeGroup *g) : m_calls(c), m_group(g) {}
public Q_SLOTS:
    QDBusObjectPath EntryGroupNew() { m_calls->add(QStringLiteral("New")); return QDBusObjectPath(QStringLiteral("/Client1/EntryGroup1")); }
    QString GetAlternativeServiceName(const QString &name) { m_calls->add(QStringLiteral("Alt:") + name); return name + QStringLiteral(" #2"); }
private:
    Calls *m_calls;
    FakeGroup *m_group;
};

// Owns the fake daemon on a worker thread; start/stop emulate a daemon restart.
class FakeAvahiThread : public QThread {
public:
    explicit FakeAvahiThread(Calls *c) : m_calls(c) {}
    void run() override
    {
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-avahi"));
        FakeGroup group(m_calls, bus);
        FakeServer server(m_calls, &group);
        bus.registerObject(QStringLiteral("/"), &server, QDBusConnection::ExportAllSlots);
        bus.registerObject(QStringLiteral("/Client1/EntryGroup1"), &group,
                           QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
        bus.registerService(QStringLiteral("org.freedesktop.Avahi"));
        exec();
        bus.unregisterService(QStringLiteral("org.freedesktop.Avahi"));
        bus.unregisterObject(QStringLiteral("/"));
        bus.unregisterObject(QStringLiteral("/Client1/EntryGroup1"));
    }
private:
    Calls *m_calls;
};

ServiceAdvert advert()
{
    ServiceAdvert a;
    a.name = QStringLiteral("host");
    a.type = QStringLiteral("_qindalink._tcp");
    a.port = 47160;
    a.txt = {{QStringLiteral("proto"), QStringLiteral("1")}, {QStringLiteral("id"), QStringLiteral("0123456789abcdef")},
             {QStringLiteral("name"), QStringLiteral("host")}};
    return a;
}
} // namespace

class LinkAvahiTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        // Only the fake needs this to receive aay; the publisher under test marshals it by hand.
        qDBusRegisterMetaType<QByteArrayList>();
    }

    void publishesTheAdvertAndWithdrawsIt()
    {
        QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
        Calls calls;
        FakeAvahiThread daemon(&calls);
        daemon.start();
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(waitUntil([&] { return bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.Avahi")); }));
        AvahiPublisher pub(bus);
        QSignalSpy states(&pub, &ServicePublisher::stateChanged);
        pub.publish(advert());
        QVERIFY(waitUntil([&] { return pub.state() == QLatin1String("published"); }));
        const QStringList log = calls.snapshot();
        QVERIFY(log.contains(QStringLiteral("Add:host:_qindalink._tcp:47160:id=0123456789abcdef;name=host;proto=1")));
        QVERIFY(log.contains(QStringLiteral("Commit")));
        pub.withdraw();
        QCOMPARE(pub.state(), QStringLiteral("off"));
        QVERIFY(calls.snapshot().contains(QStringLiteral("Free")));  // synchronous: gone before withdraw returns
        daemon.quit();
        QVERIFY(daemon.wait(5000));
    }

    void renamesOnCollision()
    {
        Calls calls;
        calls.collisionsToInject = 1;
        FakeAvahiThread daemon(&calls);
        daemon.start();
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(waitUntil([&] { return bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.Avahi")); }));
        AvahiPublisher pub(bus);
        pub.publish(advert());
        QVERIFY(waitUntil([&] { return pub.state() == QLatin1String("renamed"); }));
        const QStringList log = calls.snapshot();
        QVERIFY(log.contains(QStringLiteral("Alt:host")));
        QVERIFY(log.contains(QStringLiteral("Reset")));
        bool second = false;
        for (const QString &l : log) second |= l.startsWith(QLatin1String("Add:host #2:_qindalink._tcp:47160:"));
        QVERIFY2(second, qPrintable(log.join(QLatin1Char('|'))));
        daemon.quit();
        QVERIFY(daemon.wait(5000));
    }

    void unavailableWithoutDaemonThenRecoversWhenItAppears()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        AvahiPublisher pub(bus);
        pub.publish(advert());
        QCOMPARE(pub.state(), QStringLiteral("unavailable"));
        Calls calls;
        FakeAvahiThread daemon(&calls);
        daemon.start();
        QVERIFY(waitUntil([&] { return pub.state() == QLatin1String("published"); }, 8000));
        // A daemon restart re-creates the entry from the remembered advert.
        const qsizetype newsBefore = calls.snapshot().filter(QStringLiteral("New")).size();
        daemon.quit();
        QVERIFY(daemon.wait(5000));
        QVERIFY(waitUntil([&] { return pub.state() == QLatin1String("unavailable"); }));
        FakeAvahiThread daemon2(&calls);
        daemon2.start();
        QVERIFY(waitUntil([&] { return pub.state() == QLatin1String("published"); }, 8000));
        QVERIFY(calls.snapshot().filter(QStringLiteral("New")).size() > newsBefore);
        daemon2.quit();
        QVERIFY(daemon2.wait(5000));
    }
};

QTEST_GUILESS_MAIN(LinkAvahiTest)
#include "tst_link_avahi.moc"

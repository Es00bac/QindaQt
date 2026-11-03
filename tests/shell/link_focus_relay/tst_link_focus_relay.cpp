// SPDX-License-Identifier: GPL-3.0-or-later
// The shell's QindaLink focus relay (ADR-0440) against a fake org.qindaqt.Link1 on a private bus
// (dbus-run-session). The fake records what Focus.Publish received; no real service, portal or title
// leaves this process.
#include "linkfocusrelay.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using QindaQt::Shell::LinkFocusRelay;
namespace Compositor = QindaQt::Compositor;

namespace {

class FakeLink1 : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Focus")

public:
    explicit FakeLink1(QDBusConnection bus) : m_bus(std::move(bus))
    {
        m_bus.registerObject(QStringLiteral("/org/qindaqt/Link1"), this, QDBusConnection::ExportAllSlots);
    }
    bool up()
    {
        return m_bus.registerService(QStringLiteral("org.qindaqt.Link1"));
    }
    void down() { m_bus.unregisterService(QStringLiteral("org.qindaqt.Link1")); }
    QStringList received;
    bool deny = false;

public Q_SLOTS:
    void Publish(const QString &json)
    {
        received << json;
        if (deny) {
            sendErrorReply(QStringLiteral("org.qindaqt.Link1.Error.Denied"), QStringLiteral("denied"));
        }
    }

private:
    QDBusConnection m_bus;
};

Compositor::ShellWindowIdentitySnapshot snapshot(const QString &windowId, quint64 revision)
{
    Compositor::ShellWindowIdentitySnapshot s;
    s.status = Compositor::ShellWindowIdentityStatus::Ok;
    s.revision = revision;
    Compositor::ShellWindowIdentityFacts facts;
    facts.windowId = windowId;
    s.activeWindow = facts;
    return s;
}

} // namespace

class TstLinkFocusRelay : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void factJoinsIdentityWithTaskFacts();
    void publishesOnChangeOnlyAndSurvivesServiceRestart();
    void unchangedFactIsRepublishedAsHeartbeat();
    void unknownFocusPublishesNone();
    void deniedIsRetriedButWarnedOnce();
};

void TstLinkFocusRelay::factJoinsIdentityWithTaskFacts()
{
    const LinkFocusRelay::WindowFacts facts = [](const QString &id, QString *app, QString *title) {
        if (id != QLatin1String("{w1}")) return false;
        *app = QStringLiteral("org.example.term");
        *title = QStringLiteral("Terminal");
        return true;
    };
    const QJsonObject o = QJsonDocument::fromJson(
        LinkFocusRelay::factJson(snapshot(QStringLiteral("{w1}"), 9), facts).toUtf8()).object();
    QCOMPARE(o.value(QStringLiteral("uuid")).toString(), QStringLiteral("{w1}"));
    QCOMPARE(o.value(QStringLiteral("appId")).toString(), QStringLiteral("org.example.term"));
    QCOMPARE(o.value(QStringLiteral("title")).toString(), QStringLiteral("Terminal"));
    QCOMPARE(o.value(QStringLiteral("revision")).toInt(), 9);
    // Every kind of "not known" is the empty fact, never a guess.
    const QString none = QStringLiteral("{}");
    QCOMPARE(LinkFocusRelay::factJson(std::nullopt, facts), none);
    QCOMPARE(LinkFocusRelay::factJson(snapshot(QStringLiteral("{unknown}"), 1), facts), none);
    QCOMPARE(LinkFocusRelay::factJson(snapshot(QStringLiteral("{w1}"), 1), nullptr), none);
    auto unavailable = snapshot(QStringLiteral("{w1}"), 1);
    unavailable.status = Compositor::ShellWindowIdentityStatus::Unauthorized;
    QCOMPARE(LinkFocusRelay::factJson(unavailable, facts), none);
    auto noWindow = snapshot(QStringLiteral("{w1}"), 1);
    noWindow.activeWindow.reset();
    QCOMPARE(LinkFocusRelay::factJson(noWindow, facts), none);
}

void TstLinkFocusRelay::publishesOnChangeOnlyAndSurvivesServiceRestart()
{
    QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
    QDBusConnection serviceBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-link1"));
    FakeLink1 fake(serviceBus);
    QString current = QStringLiteral("{\"uuid\":\"a\"}");
    LinkFocusRelay relay(QDBusConnection::sessionBus(), [&] { return current; });
    relay.start(); // the service is not up: nothing is sent, and nothing activates it
    QTest::qWait(150);
    QVERIFY(fake.received.isEmpty());
    QVERIFY(fake.up());
    QTRY_COMPARE(fake.received.size(), 1);
    QCOMPARE(fake.received.last(), current);
    relay.refresh();
    relay.refresh();
    QTest::qWait(100);
    QCOMPARE(fake.received.size(), 1); // unchanged: nothing more
    current = QStringLiteral("{\"uuid\":\"b\"}");
    relay.refresh();
    QTRY_COMPARE(fake.received.size(), 2);
    // A restarted service knows nothing, so the current fact is published again.
    fake.down();
    QTest::qWait(100);
    QVERIFY(fake.up());
    QTRY_COMPARE(fake.received.size(), 3);
    QCOMPARE(fake.received.last(), current);
    fake.down();
}

void TstLinkFocusRelay::unchangedFactIsRepublishedAsHeartbeat()
{
    QDBusConnection serviceBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-link1-h"));
    FakeLink1 fake(serviceBus);
    QVERIFY(fake.up());
    LinkFocusRelay relay(QDBusConnection::sessionBus(), [] { return QStringLiteral("{\"uuid\":\"a\"}"); });
    relay.start();
    QTRY_VERIFY_WITH_TIMEOUT(fake.received.size() >= 3, 4000); // the service drops a fact older than 3 s, so it must keep coming
    QCOMPARE(fake.received.first(), fake.received.last());
    fake.down();
}

void TstLinkFocusRelay::unknownFocusPublishesNone()
{
    QDBusConnection serviceBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-link1-b"));
    FakeLink1 fake(serviceBus);
    QVERIFY(fake.up());
    QString current = QStringLiteral("{\"uuid\":\"a\"}");
    LinkFocusRelay relay(QDBusConnection::sessionBus(), [&] { return current; });
    relay.start();
    QTRY_COMPARE(fake.received.size(), 1);
    current = QStringLiteral("{}");
    relay.refresh();
    QTRY_COMPARE(fake.received.size(), 2);
    QCOMPARE(fake.received.last(), QStringLiteral("{}"));
    fake.down();
}

void TstLinkFocusRelay::deniedIsRetriedButWarnedOnce()
{
    QDBusConnection serviceBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-link1-c"));
    FakeLink1 fake(serviceBus);
    fake.deny = true;
    QVERIFY(fake.up());
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("denied Focus.Publish")));
    LinkFocusRelay relay(QDBusConnection::sessionBus(), [] { return QStringLiteral("{\"uuid\":\"a\"}"); });
    relay.start();
    QTRY_COMPARE(fake.received.size(), 1);
    relay.refresh(); // not accepted, so it is tried again; the warning is not repeated (a second one would show in the log)
    QTRY_VERIFY(fake.received.size() >= 2);
    fake.down();
}

QTEST_MAIN(TstLinkFocusRelay)
#include "tst_link_focus_relay.moc"

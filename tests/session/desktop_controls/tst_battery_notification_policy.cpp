// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/battery_notification_policy.h>
#include <qindaqt/session/desktop_controls/freedesktop_feedback_notifier.h>
#include <qindaqt/services/power_client/power_client.h>

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest>

using namespace QindaQt::Power;
using namespace QindaQt::Session::DesktopControls;

namespace {

const QString kOwner = QStringLiteral(":1.7");

// Minimal fake power transport: only what the policy's drive-to-Ready and
// snapshot-replacement path needs. tests/services/power_client/support
// carries a richer fixture for the client's own contract tests; this one is
// deliberately local and battery-only so a change to that fixture cannot
// silently change what this policy test proves.
class FakePowerTransport final : public PowerTransport {
public:
    using PowerTransport::PowerTransport;

    struct Fetch { QString owner; quint64 requestId = 0; };

    void start() override {}
    void stop() override {}
    void fetchSnapshot(const QString &owner, const quint64 requestId) override {
        fetches.push_back({owner, requestId});
    }
    void submitOperation(const QString &, const quint64,
                         const PowerClientRequest &) override {}

    void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
    void reply(const Fetch &fetch, const Snapshot &snapshot) {
        Q_EMIT snapshotReply(fetch.owner, fetch.requestId, true, snapshot, {});
    }

    QList<Fetch> fetches;
};

// Minimal org.freedesktop.Notifications fake: only records what this policy
// test asserts (summary, whether it replaced the previous battery popup,
// and expire_timeout/urgency), not the full wire shape tst_freedesktop_
// feedback_notifier.cpp already covers.
class FakeNotificationService final : public QDBusVirtualObject {
public:
    struct Call { quint32 replacesId = 0; QString summary; QString body; int expireTimeout = -1; quint8 urgency = 0; };

    explicit FakeNotificationService(const QDBusConnection &connection)
        : m_connection(connection) {}

    bool registerService() {
        if (!m_connection.registerService(QStringLiteral("org.freedesktop.Notifications"))) {
            return false;
        }
        return m_connection.registerVirtualObject(
            QStringLiteral("/org/freedesktop/Notifications"), this);
    }

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override {
        if (message.interface() != QStringLiteral("org.freedesktop.Notifications")
            || message.member() != QStringLiteral("Notify")) {
            return false;
        }
        const QVariantList arguments = message.arguments();
        if (arguments.size() != 8) {
            return false;
        }
        Call call;
        call.replacesId = arguments.at(1).toUInt();
        call.summary = arguments.at(3).toString();
        call.body = arguments.at(4).toString();
        const QVariantMap hints = qdbus_cast<QVariantMap>(arguments.at(6));
        call.urgency = hints.value(QStringLiteral("urgency")).value<quint8>();
        call.expireTimeout = arguments.at(7).toInt();
        calls.append(call);

        QDBusMessage reply = message.createReply();
        reply << ++m_lastId;
        connection.send(reply);
        return true;
    }

    QList<Call> calls;
    quint32 m_lastId = 0;

private:
    QDBusConnection m_connection;
};

Snapshot batterySnapshot(const double percent, const WarningLevel warning,
                         const bool present = true) {
    Snapshot snapshot;
    snapshot.epoch = 1;
    snapshot.revision = 1;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capability::Supplies;
    snapshot.source = {.acPresent = !present, .onBattery = present};
    snapshot.composite = {.present = present,
                          .sourceCount = present ? 1U : 0U,
                          .percentageKnown = true,
                          .percentage = percent,
                          .timeToEmptyKnown = true,
                          .timeToEmptySeconds = 1'800,
                          .warning = warning};
    return snapshot;
}

} // namespace

class BatteryNotificationPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void firesOnceOnCrossingIntoLow();
    void criticalReplacesTheLowPopupAndRaisesUrgency();
    void recoveringAboveLowAllowsANewLowNotification();
    void noBatteryNeverNotifies();

private:
    QDBusConnection m_connection{QDBusConnection::sessionBus()};
};

void BatteryNotificationPolicyTest::firesOnceOnCrossingIntoLow() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);
    FakePowerTransport transport;
    PowerClient client(&transport);
    BatteryNotificationPolicy policy(client, notifier);
    policy.start();

    client.start();
    transport.announceOwner(kOwner);
    QVERIFY(!transport.fetches.isEmpty());
    transport.reply(transport.fetches.constLast(),
                    batterySnapshot(45.0, WarningLevel::Discharging));
    QCOMPARE(policy.lastNotifiedLevel(), WarningLevel::None);
    QVERIFY(service.calls.isEmpty());

    // Crossing into Low notifies once. Later snapshots arrive as plain
    // signal emissions here (PowerClient's own reply-matching/refetch wire
    // protocol is tst_power_client.cpp's contract, not this policy's).
    Q_EMIT client.snapshotChanged(batterySnapshot(20.0, WarningLevel::Low));
    QTRY_COMPARE(service.calls.size(), 1);
    QCOMPARE(service.calls.constFirst().summary, QStringLiteral("Battery low"));
    QCOMPARE(service.calls.constFirst().body, QStringLiteral("20% · 30 minutes remaining"));
    QCOMPARE(service.calls.constFirst().expireTimeout, 0);
    QCOMPARE(service.calls.constFirst().urgency, quint8(1));
    QCOMPARE(policy.lastNotifiedLevel(), WarningLevel::Low);

    // A second snapshot still at Low must not notify again.
    Q_EMIT client.snapshotChanged(batterySnapshot(19.0, WarningLevel::Low));
    QCOMPARE(service.calls.size(), 1);
}

void BatteryNotificationPolicyTest::criticalReplacesTheLowPopupAndRaisesUrgency() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);
    FakePowerTransport transport;
    PowerClient client(&transport);
    BatteryNotificationPolicy policy(client, notifier);
    policy.start();
    client.start();
    transport.announceOwner(kOwner);

    Q_EMIT client.snapshotChanged(batterySnapshot(20.0, WarningLevel::Low));
    QTRY_COMPARE(service.calls.size(), 1);
    QTest::qWait(50);
    const quint32 lowId = service.m_lastId;

    Q_EMIT client.snapshotChanged(batterySnapshot(5.0, WarningLevel::Critical));
    QTRY_COMPARE(service.calls.size(), 2);
    QCOMPARE(service.calls.at(1).replacesId, lowId);
    QCOMPARE(service.calls.at(1).summary, QStringLiteral("Battery critical"));
    QCOMPARE(service.calls.at(1).urgency, quint8(2));
    QCOMPARE(policy.lastNotifiedLevel(), WarningLevel::Critical);
}

void BatteryNotificationPolicyTest::recoveringAboveLowAllowsANewLowNotification() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);
    FakePowerTransport transport;
    PowerClient client(&transport);
    BatteryNotificationPolicy policy(client, notifier);
    policy.start();
    client.start();
    transport.announceOwner(kOwner);

    Q_EMIT client.snapshotChanged(batterySnapshot(20.0, WarningLevel::Low));
    QTRY_COMPARE(service.calls.size(), 1);
    QCOMPARE(policy.lastNotifiedLevel(), WarningLevel::Low);

    // Plugged in and charging: warning drops below Low, the edge resets.
    Q_EMIT client.snapshotChanged(batterySnapshot(35.0, WarningLevel::None));
    QCOMPARE(policy.lastNotifiedLevel(), WarningLevel::None);
    QCOMPARE(service.calls.size(), 1);

    // Unplugged again and back down to Low: notifies a second time.
    Q_EMIT client.snapshotChanged(batterySnapshot(20.0, WarningLevel::Low));
    QTRY_COMPARE(service.calls.size(), 2);
}

void BatteryNotificationPolicyTest::noBatteryNeverNotifies() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);
    FakePowerTransport transport;
    PowerClient client(&transport);
    BatteryNotificationPolicy policy(client, notifier);
    policy.start();
    client.start();
    transport.announceOwner(kOwner);

    Q_EMIT client.snapshotChanged(
        batterySnapshot(0.0, WarningLevel::Unknown, /*present=*/false));
    QTest::qWait(50);
    QVERIFY(service.calls.isEmpty());
    QCOMPARE(policy.lastNotifiedLevel(), WarningLevel::None);
}

QTEST_MAIN(BatteryNotificationPolicyTest)
#include "tst_battery_notification_policy.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/shortcuts_client/shortcut_lease.h>
#include <QDBusContext>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>
using namespace QindaQt::Services::Shortcuts;
class Native final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Shortcuts1")
public:
    QVariantMap rows; int delay = 0; int registrations = 0;
    static QString id(const QString &component, const QString &action) { return component + QChar(0x1f) + action; }
public Q_SLOTS:
    QVariantMap ListBindings() const { return rows; }
    bool Register(const QString &component, const QString &action, const QString &description, const QStringList &keys, bool repeat, bool transient, QString &reason) {
        Q_UNUSED(repeat); Q_UNUSED(transient); ++registrations;
        if (keys.contains("Alt+Tab")) { reason = "window shortcut"; return false; }
        rows[id(component, action)] = QVariantMap{{"component", component}, {"action", action}, {"description", description}, {"keys", keys}, {"active", true}};
        if (delay) {
            setDelayedReply(true); const auto call = message(); const auto bus = connection();
            QTimer::singleShot(delay, this, [call, bus] { bus.send(call.createReply({true, QString{}})); });
        }
        Q_EMIT BindingsChanged(); return true;
    }
    bool SetShortcuts(const QString &component, const QString &action, const QStringList &keys, QString &reason) {
        if (keys.contains("Alt+Tab")) { reason = "window shortcut"; return false; }
        auto row = rows.value(id(component, action)).toMap(); row["keys"] = keys; rows[id(component, action)] = row; Q_EMIT BindingsChanged(); return true;
    }
    bool Unregister(const QString &component, const QString &action) { const bool removed = rows.remove(id(component, action)); Q_EMIT BindingsChanged(); return removed; }
Q_SIGNALS:
    void Activated(const QString &component, const QString &action, quint64 timestamp);
    void BindingsChanged();
};
class Release final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KGlobalAccel")
public:
    explicit Release(Native &native) : native(native) {}
    Native &native;
public Q_SLOTS:
    void setInactive(const QStringList &id) { auto row = native.rows.value(Native::id(id[0], id[1])).toMap(); row["active"] = false; native.rows[Native::id(id[0], id[1])] = row; }
};
class ShortcutLeaseTest final : public QObject {
    Q_OBJECT
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnection provider = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "shortcut-lease-provider");
    Native native;
    Release release{native};
    QString status(const QSignalSpy &spy, const QString &id) {
        for (auto i = spy.crbegin(); i != spy.crend(); ++i) if (i->at(0).toString() == id) return i->at(1).toString();
        return {};
    }
    Binding binding(const QString &id, const QString &key) { Binding b; b.component = "test-macros"; b.action = id; b.description = id; if (!key.isEmpty()) b.keys = {QKeySequence(key)}; return b; }
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(provider.registerService(QLatin1String(Service)));
        QVERIFY(provider.registerObject(QLatin1String(Path), &native, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        QVERIFY(provider.registerObject("/kglobalaccel", &release, QDBusConnection::ExportAllSlots));
    }
    void init() { native.rows.clear(); native.delay = 0; native.registrations = 0; }
    void unconfiguredLeaseCannotEraseAssignmentsWhenLibraryLoadFails() {
        const auto key = Native::id("test-macros", "keep");
        native.rows[key] = QVariantMap{{"component", "test-macros"}, {"action", "keep"}, {"keys", QStringList{"Meta+F13"}}, {"active", false}};
        const auto before = native.rows;
        { ShortcutLease lease(bus, "test-macros"); QTest::qWait(20); QCOMPARE(native.registrations, 0); QCOMPARE(native.rows, before); }
        QTest::qWait(10); QCOMPARE(native.rows, before);
    }
    void existingNativeAlternativesSurviveReclaimEditsAndTeardown() {
        const auto key = Native::id("test-macros", "copy");
        native.rows[key] = QVariantMap{{"component", "test-macros"}, {"action", "copy"}, {"keys", QStringList{"Meta+F13", "Meta+F14"}}, {"active", false}};
        {
            ShortcutLease lease(bus, "test-macros"); QSignalSpy states(&lease, &ShortcutLease::status), activated(&lease, &ShortcutLease::activated);
            lease.replace({binding("copy", "Meta+F15")}); QTRY_COMPARE(status(states, "copy"), "ready");
            QCOMPARE(native.rows[key].toMap().value("keys").toStringList(), (QStringList{"Meta+F13", "Meta+F14"}));
            Q_EMIT native.Activated("test-macros", "copy", 123); QTRY_COMPARE(activated.size(), 1); QCOMPARE(activated.first().at(1).toULongLong(), quint64(123));
            lease.replace({binding("copy", "Meta+F15")}, {"copy"});
            QTRY_COMPARE(native.rows[key].toMap().value("keys").toStringList(), QStringList{"Meta+F15"});
        }
        QTRY_VERIFY(!native.rows[key].toMap().value("active").toBool());
        QCOMPARE(native.rows[key].toMap().value("keys").toStringList(), QStringList{"Meta+F15"});
    }
    void delayedProviderLeavesTheEventLoopResponsiveAndStaleRepliesCannotActivate() {
        native.delay = 100; ShortcutLease lease(bus, "test-macros"); QSignalSpy activated(&lease, &ShortcutLease::activated), states(&lease, &ShortcutLease::status);
        int heartbeats = 0; QTimer pulse; pulse.setInterval(5); connect(&pulse, &QTimer::timeout, this, [&] { ++heartbeats; }); pulse.start();
        lease.replace({binding("old", "Meta+F13")}); QTRY_VERIFY(native.registrations > 0);
        lease.replace({binding("new", "Meta+F14")}); Q_EMIT native.Activated("test-macros", "old", 456);
        QTRY_COMPARE(status(states, "new"), "ready"); QVERIFY(heartbeats > 5); QCOMPARE(activated.size(), 0);
        QVERIFY(!native.rows.contains(Native::id("test-macros", "old")));
    }
    void conflictsAndOwnerLossCannotClaimActivation() {
        ShortcutLease lease(bus, "test-macros"); QSignalSpy states(&lease, &ShortcutLease::status), activated(&lease, &ShortcutLease::activated);
        lease.replace({binding("conflict", "Alt+Tab")}); QTRY_COMPARE(status(states, "conflict"), "conflict");
        Q_EMIT native.Activated("test-macros", "conflict", 789); QTest::qWait(10); QCOMPARE(activated.size(), 0);
        QVERIFY(provider.unregisterService(QLatin1String(Service))); QTRY_COMPARE(status(states, "conflict"), "unavailable");
        Q_EMIT native.Activated("test-macros", "conflict", 790); QTest::qWait(10); QCOMPARE(activated.size(), 0);
        QVERIFY(provider.registerService(QLatin1String(Service)));
    }
};
QTEST_GUILESS_MAIN(ShortcutLeaseTest)
#include "tst_shortcut_lease.moc"

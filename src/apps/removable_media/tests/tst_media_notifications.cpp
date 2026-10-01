// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_notifications.h"
#include <QDBusContext>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Apps::RemovableMedia;
static const QString Service = QStringLiteral("org.freedesktop.Notifications");
static const QString Path = QStringLiteral("/org/freedesktop/Notifications");

class FixtureBackend final : public MediaBackend {
public:
    QVector<Volume> rows;
    QList<Request> requests;
    QVector<Volume> volumes() const override { return rows; }
    bool available() const override { return true; }
    QString diagnostic() const override { return {}; }
    QStringList formatTypes() const override { return {}; }
    void refresh() override { Q_EMIT changed(); }
    void execute(const Request &request) override { requests.append(request); }
};
class NotificationServer final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    QList<QDBusMessage> pending;
    QList<uint> closedIds;
public Q_SLOTS:
    uint Notify(const QString &, uint, const QString &, const QString &, const QString &,
                const QStringList &, const QVariantMap &, int) {
        setDelayedReply(true); pending.append(message()); return 0;
    }
    void CloseNotification(uint id) { closedIds.append(id); }
Q_SIGNALS:
    void ActionInvoked(uint id, const QString &action);
};

class MediaNotificationsTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void ownerLossDuringInsertionOpensWindow() {
        withFixture([&](FixtureBackend &, MediaController &controller, NotificationServer &endpoint,
                        QDBusConnection &server) {
            QSignalSpy windows(&controller, &MediaController::windowRequested);
            QTRY_COMPARE(endpoint.pending.size(), 1); QCOMPARE(windows.size(), 0);
            QVERIFY(server.unregisterService(Service));
            QTRY_COMPARE(windows.size(), 1);
            QVERIFY(server.send(endpoint.pending.constFirst().createReply(QVariantList{uint(7)})));
            QTest::qWait(20); QCOMPARE(windows.size(), 1);
        });
    }
    void supersededUpdateDoesNotCloseSharedCurrentId() {
        withFixture([&](FixtureBackend &backend, MediaController &controller, NotificationServer &endpoint,
                        QDBusConnection &server) {
            QTRY_COMPARE(endpoint.pending.size(), 1);
            QVERIFY(server.send(endpoint.pending[0].createReply(QVariantList{uint(7)})));
            QTest::qWait(20);
            const QString token = backend.rows.constFirst().token;
            Q_EMIT controller.notificationRequested(token, QStringLiteral("Update one"), {}, {});
            QTRY_COMPARE(endpoint.pending.size(), 2);
            Q_EMIT controller.notificationRequested(token, QStringLiteral("Update two"), {}, {});
            QTRY_COMPARE(endpoint.pending.size(), 3);
            QCOMPARE(endpoint.pending[1].arguments().at(1).toUInt(), uint(7));
            QCOMPARE(endpoint.pending[2].arguments().at(1).toUInt(), uint(7));
            QVERIFY(server.send(endpoint.pending[1].createReply(QVariantList{uint(7)})));
            QTest::qWait(20);
            QVERIFY(server.send(endpoint.pending[2].createReply(QVariantList{uint(7)})));
            QTest::qWait(20);
            QVERIFY(!endpoint.closedIds.contains(uint(7)));
            QSignalSpy windows(&controller, &MediaController::windowRequested);
            Q_EMIT endpoint.ActionInvoked(7, QStringLiteral("more"));
            QTRY_COMPARE(windows.size(), 1);
        });
    }
    void withdrawalClosesEventualIdAndRejectsItsActions() {
        withFixture([&](FixtureBackend &backend, MediaController &controller, NotificationServer &endpoint,
                        QDBusConnection &server) {
            QSignalSpy windows(&controller, &MediaController::windowRequested);
            QTRY_COMPARE(endpoint.pending.size(), 1);
            backend.rows.clear(); Q_EMIT backend.changed();
            QVERIFY(server.send(endpoint.pending[0].createReply(QVariantList{uint(7)})));
            QTRY_VERIFY(endpoint.closedIds.contains(uint(7)));
            Q_EMIT endpoint.ActionInvoked(7, QStringLiteral("mount"));
            QTest::qWait(20);
            QCOMPARE(windows.size(), 0); QVERIFY(backend.requests.isEmpty());
        });
    }
    void withdrawalBeforeOwnerLossDoesNotOpenStaleWindow() {
        withFixture([&](FixtureBackend &backend, MediaController &controller, NotificationServer &endpoint,
                        QDBusConnection &server) {
            QSignalSpy windows(&controller, &MediaController::windowRequested);
            QDBusServiceWatcher watcher(Service, QDBusConnection::sessionBus(),
                                        QDBusServiceWatcher::WatchForOwnerChange);
            QSignalSpy owners(&watcher, &QDBusServiceWatcher::serviceOwnerChanged);
            QTRY_COMPARE(endpoint.pending.size(), 1);
            backend.rows.clear(); Q_EMIT backend.changed();
            QVERIFY(server.unregisterService(Service)); QTRY_COMPARE(owners.size(), 1);
            QVERIFY(server.send(endpoint.pending[0].createReply(QVariantList{uint(7)})));
            QTest::qWait(20); QCOMPARE(windows.size(), 0);
        });
    }
private:
    template<class Function> void withFixture(Function function) {
        auto server = QDBusConnection::connectToBus(QString::fromUtf8(qgetenv("DBUS_SESSION_BUS_ADDRESS")),
                                                   QStringLiteral("media-notification-server"));
        QVERIFY(server.isConnected()); QVERIFY(server.registerService(Service));
        NotificationServer endpoint;
        QVERIFY(server.registerObject(Path, &endpoint,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        QTemporaryDir directory; QVERIFY(directory.isValid());
        {
            MediaPreferences preferences(directory.filePath(QStringLiteral("choices.json")));
            FixtureBackend backend;
            Volume volume; volume.token = QStringLiteral("attachment-1");
            volume.label = QStringLiteral("Inserted USB"); volume.mountable = true;
            backend.rows.append(volume);
            MediaController controller(backend, preferences);
            MediaNotifications notifications(controller, QDBusConnection::sessionBus());
            function(backend, controller, endpoint, server);
        }
        server.unregisterService(Service); server.unregisterObject(Path);
        QDBusConnection::disconnectFromBus(QStringLiteral("media-notification-server"));
    }
};
QTEST_GUILESS_MAIN(MediaNotificationsTest)
#include "tst_media_notifications.moc"

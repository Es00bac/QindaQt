// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/freedesktop_feedback_notifier.h>

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest>

#include <optional>

using namespace QindaQt::Session::DesktopControls;

namespace {

// Minimal org.freedesktop.Notifications fake as a virtual object so the
// notifier's real wire shape (argument order, replaces-id reuse, expire hint)
// is exercised against a private bus.
class FakeNotificationService final : public QDBusVirtualObject {
public:
    struct NotifyCall {
        QString appName;
        quint32 replacesId = 0;
        QString iconName;
        QString summary;
        QString body;
        QVariantMap hints;
        int expireTimeout = -1;
    };

    explicit FakeNotificationService(const QDBusConnection &connection)
        : m_connection(connection)
    {
    }

    bool registerService()
    {
        if (!m_connection.registerService(QStringLiteral("org.freedesktop.Notifications"))) {
            return false;
        }
        return m_connection.registerVirtualObject(QStringLiteral("/org/freedesktop/Notifications"),
                                                  this);
    }

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.interface() != QStringLiteral("org.freedesktop.Notifications")
            || message.member() != QStringLiteral("Notify")) {
            return false;
        }
        const QVariantList arguments = message.arguments();
        if (arguments.size() != 8) {
            return false;
        }
        NotifyCall call;
        call.appName = arguments.at(0).toString();
        call.replacesId = arguments.at(1).toUInt();
        call.iconName = arguments.at(2).toString();
        call.summary = arguments.at(3).toString();
        call.body = arguments.at(4).toString();
        call.hints = qdbus_cast<QVariantMap>(arguments.at(6));
        call.expireTimeout = arguments.at(7).toInt();
        calls.append(call);

        QDBusMessage reply = message.createReply();
        reply << ++m_lastId;
        connection.send(reply);
        return true;
    }

    QList<NotifyCall> calls;
    quint32 m_lastId = 0;

private:
    QDBusConnection m_connection;
};

} // namespace

class FreedesktopFeedbackNotifierTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void volumeFeedbackReplacesOnePopupPerCategory();
    void brightnessFeedbackUsesItsOwnPopup();
    void mutedVolumeShowsMutedBodyAndIcon();
    void absentServiceReportsFailureAndKeepsWorking();
    void batteryFeedbackPersistsAndCarriesUrgency();

private:
    QDBusConnection m_connection{QDBusConnection::sessionBus()};
};

void FreedesktopFeedbackNotifierTest::volumeFeedbackReplacesOnePopupPerCategory() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);

    notifier.showVolume(55, false);
    QTRY_COMPARE(service.calls.size(), 1);
    // Wait for the reply to reach the notifier so the stored id is observed;
    // there is deliberately no public completion API on the notifier.
    QTest::qWait(50);
    const auto &first = service.calls.constFirst();
    const quint32 firstId = service.m_lastId;
    QCOMPARE(first.appName, QStringLiteral("QindaQt"));
    QCOMPARE(first.replacesId, 0U);
    QCOMPARE(first.summary, QStringLiteral("Volume"));
    QCOMPARE(first.body, QStringLiteral("55%"));
    QCOMPARE(first.iconName, QStringLiteral("audio-volume-medium"));
    QVERIFY(first.expireTimeout > 0);

    notifier.showVolume(60, false);
    QTRY_COMPARE(service.calls.size(), 2);
    // The second call replaces the first popup instead of stacking a new one.
    QCOMPARE(service.calls.at(1).replacesId, firstId);
    QCOMPARE(service.calls.at(1).body, QStringLiteral("60%"));
}

void FreedesktopFeedbackNotifierTest::brightnessFeedbackUsesItsOwnPopup() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);

    notifier.showVolume(40, false);
    QTRY_COMPARE(service.calls.size(), 1);
    QTest::qWait(50);
    const quint32 volumeId = service.m_lastId;
    notifier.showBrightness(70);
    QTRY_COMPARE(service.calls.size(), 2);
    QTest::qWait(50);
    // Brightness is a separate category: its first popup does not replace the
    // volume popup.
    QCOMPARE(service.calls.at(1).replacesId, 0U);
    QCOMPARE(service.calls.at(1).summary, QStringLiteral("Brightness"));
    QCOMPARE(service.calls.at(1).body, QStringLiteral("70%"));
    QCOMPARE(service.calls.at(1).iconName, QStringLiteral("video-display"));
    const quint32 brightnessId = service.m_lastId;
    QVERIFY(brightnessId != volumeId);
    notifier.showBrightness(75);
    QTRY_COMPARE(service.calls.size(), 3);
    QCOMPARE(service.calls.at(2).replacesId, brightnessId);
}

void FreedesktopFeedbackNotifierTest::mutedVolumeShowsMutedBodyAndIcon() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);

    notifier.showVolume(40, true);
    QTRY_COMPARE(service.calls.size(), 1);
    QCOMPARE(service.calls.constFirst().body, QStringLiteral("Muted"));
    QCOMPARE(service.calls.constFirst().iconName, QStringLiteral("audio-volume-muted"));
}

void FreedesktopFeedbackNotifierTest::absentServiceReportsFailureAndKeepsWorking() {
    {
        // No fake service is registered in this scope; the notification host
        // is absent.
        FreedesktopFeedbackNotifier notifier(m_connection);
        QSignalSpy failed(&notifier, &FreedesktopFeedbackNotifier::notificationFailed);
        notifier.showVolume(50, false);
        QVERIFY(failed.wait(2'000));
    }
    {
        FakeNotificationService service(m_connection);
        QVERIFY(service.registerService());
        FreedesktopFeedbackNotifier notifier(m_connection);
        notifier.showVolume(50, false);
        QTRY_COMPARE(service.calls.size(), 1);
        QCOMPARE(service.calls.constFirst().replacesId, 0U);
    }
}

void FreedesktopFeedbackNotifierTest::batteryFeedbackPersistsAndCarriesUrgency() {
    FakeNotificationService service(m_connection);
    QVERIFY(service.registerService());
    FreedesktopFeedbackNotifier notifier(m_connection);

    notifier.showBattery(QStringLiteral("Battery low"), QStringLiteral("20%"),
                         QStringLiteral("battery-low"), false);
    QTRY_COMPARE(service.calls.size(), 1);
    QTest::qWait(50);
    const auto &low = service.calls.constFirst();
    QCOMPARE(low.summary, QStringLiteral("Battery low"));
    // 0 = never auto-expire (freedesktop spec), unlike the 1.2 s OSD flash.
    QCOMPARE(low.expireTimeout, 0);
    QCOMPARE(low.hints.value(QStringLiteral("urgency")).toUInt(), 1U);
    const quint32 lowId = service.m_lastId;

    notifier.showBattery(QStringLiteral("Battery critical"),
                         QStringLiteral("5%"),
                         QStringLiteral("battery-caution"), true);
    QTRY_COMPARE(service.calls.size(), 2);
    // Battery is its own category: replaces the previous battery popup, not
    // volume/brightness/notice, and critical carries urgency 2.
    QCOMPARE(service.calls.at(1).replacesId, lowId);
    QCOMPARE(service.calls.at(1).hints.value(QStringLiteral("urgency")).toUInt(),
             2U);
    QCOMPARE(service.calls.at(1).expireTimeout, 0);
}

QTEST_MAIN(FreedesktopFeedbackNotifierTest)
#include "tst_freedesktop_feedback_notifier.moc"

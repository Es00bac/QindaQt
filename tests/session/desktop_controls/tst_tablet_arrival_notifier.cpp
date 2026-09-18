// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/desktop_controls/tablet_arrival_notifier.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QProcess>
#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

using QindaQt::Session::DesktopControls::TabletArrivalNotifier;

namespace {

// Private dbus-daemon fixture. No row here contacts the host session bus or
// the user's real notification host.
class PrivateBus final {
public:
    bool start() {
        process.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        process.setArguments({QStringLiteral("--session"),
                              QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-tablet-notify-test-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }

    ~PrivateBus() {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
};

// Minimal org.freedesktop.Notifications host: it records what it was asked
// to show and can invoke an action back at the caller.
class FakeNotificationHost final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    struct Shown {
        quint32 replacesId = 0;
        QString summary;
        QString body;
        QStringList actions;
    };
    QList<Shown> shown;
    quint32 nextId = 100;

    bool publish(QDBusConnection bus) {
        m_bus = bus;
        return bus.registerService(QStringLiteral("org.freedesktop.Notifications")) &&
               bus.registerObject(QStringLiteral("/org/freedesktop/Notifications"),
                                  this, QDBusConnection::ExportAllContents);
    }

    void invoke(quint32 id, const QString &actionKey) {
        auto message = QDBusMessage::createSignal(
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("ActionInvoked"));
        message.setArguments({id, actionKey});
        m_bus.send(message);
    }

public Q_SLOTS:
    Q_SCRIPTABLE uint Notify(const QString &, uint replacesId, const QString &,
                             const QString &summary, const QString &body,
                             const QStringList &actions, const QVariantMap &,
                             int) {
        shown.append(Shown{replacesId, summary, body, actions});
        return replacesId != 0 ? replacesId : nextId++;
    }

private:
    QDBusConnection m_bus{QStringLiteral("invalid")};
};

} // namespace

class TabletArrivalNotifierTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void announcesTheTabletWithItsThreeActions();
    void theSetupActionNamesTheDeviceGroupItBelongsTo();
    void anotherApplicationsActionIsIgnored();
    void aReannouncementReplacesItsOwnPopup();
    void bodyTextSaysWhichScreenOrThatThereIsNone();
};

void TabletArrivalNotifierTest::announcesTheTabletWithItsThreeActions() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeNotificationHost host;
    QVERIFY(host.publish(bus.connection));

    TabletArrivalNotifier notifier(bus.connection);
    QString error;
    QVERIFY2(notifier.start(&error), qPrintable(error));
    notifier.announce(QStringLiteral("group-wacom"),
                      QStringLiteral("Wacom One Pen Display 13 Pen"),
                      QStringLiteral("HDMI-A-1"));
    QTRY_COMPARE(host.shown.size(), 1);
    QVERIFY(host.shown.at(0).summary.contains(QStringLiteral("Wacom")));
    QVERIFY(host.shown.at(0).body.contains(QStringLiteral("HDMI-A-1")));
    // Key, label, key, label, key, label — the exact contract with the host.
    QCOMPARE(host.shown.at(0).actions.size(), 6);
    QCOMPARE(host.shown.at(0).actions.at(0),
             TabletArrivalNotifier::setupActionKey());
    QCOMPARE(host.shown.at(0).actions.at(2),
             TabletArrivalNotifier::useInternalActionKey());
    QCOMPARE(host.shown.at(0).actions.at(4),
             TabletArrivalNotifier::dismissActionKey());
}

void TabletArrivalNotifierTest::theSetupActionNamesTheDeviceGroupItBelongsTo() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeNotificationHost host;
    QVERIFY(host.publish(bus.connection));

    TabletArrivalNotifier notifier(bus.connection);
    QVERIFY(notifier.start());
    QSignalSpy setup(&notifier, &TabletArrivalNotifier::setupRequested);
    QSignalSpy internal(&notifier,
                        &TabletArrivalNotifier::useActiveScreenRequested);

    QSignalSpy shown(&notifier, &TabletArrivalNotifier::announcementShown);
    notifier.announce(QStringLiteral("group-wacom"), QStringLiteral("Wacom"),
                      QStringLiteral("HDMI-A-1"));
    QTRY_COMPARE(shown.size(), 1);
    host.invoke(shown.at(0).at(1).toUInt(),
                TabletArrivalNotifier::setupActionKey());
    QTRY_COMPARE(setup.size(), 1);
    QCOMPARE(setup.at(0).at(0).toString(), QStringLiteral("group-wacom"));
    QCOMPARE(internal.size(), 0);
}

void TabletArrivalNotifierTest::anotherApplicationsActionIsIgnored() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeNotificationHost host;
    QVERIFY(host.publish(bus.connection));

    TabletArrivalNotifier notifier(bus.connection);
    QVERIFY(notifier.start());
    QSignalSpy setup(&notifier, &TabletArrivalNotifier::setupRequested);
    QSignalSpy shown(&notifier, &TabletArrivalNotifier::announcementShown);
    notifier.announce(QStringLiteral("group-wacom"), QStringLiteral("Wacom"),
                      QStringLiteral("HDMI-A-1"));
    QTRY_COMPARE(shown.size(), 1);

    // Every ActionInvoked on the bus reaches this process; an id this
    // notifier never owned must do nothing at all.
    host.invoke(9999, TabletArrivalNotifier::setupActionKey());
    QTest::qWait(50);
    QCOMPARE(setup.size(), 0);
}

void TabletArrivalNotifierTest::aReannouncementReplacesItsOwnPopup() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeNotificationHost host;
    QVERIFY(host.publish(bus.connection));

    TabletArrivalNotifier notifier(bus.connection);
    QVERIFY(notifier.start());
    QSignalSpy shown(&notifier, &TabletArrivalNotifier::announcementShown);
    notifier.announce(QStringLiteral("group-wacom"), QStringLiteral("Wacom"),
                      QString());
    // Wait for the host's reply, not merely for the call: the id only exists
    // once the host has answered.
    QTRY_COMPARE(shown.size(), 1);
    QCOMPARE(host.shown.size(), 1);
    QCOMPARE(host.shown.at(0).replacesId, 0u);
    QCOMPARE(shown.at(0).at(1).toUInt(), 100u);

    notifier.announce(QStringLiteral("group-wacom"), QStringLiteral("Wacom"),
                      QStringLiteral("HDMI-A-1"));
    QTRY_COMPARE(host.shown.size(), 2);
    // The second announcement updates the first popup instead of stacking.
    QCOMPARE(host.shown.at(1).replacesId, 100u);
}

void TabletArrivalNotifierTest::bodyTextSaysWhichScreenOrThatThereIsNone() {
    QVERIFY(TabletArrivalNotifier::bodyText(QStringLiteral("HDMI-A-1"))
                .contains(QStringLiteral("HDMI-A-1")));
    const QString noScreen = TabletArrivalNotifier::bodyText(QString());
    QVERIFY(!noScreen.isEmpty());
    QVERIFY(!noScreen.contains(QStringLiteral("HDMI")));
}

QTEST_MAIN(TabletArrivalNotifierTest)
#include "tst_tablet_arrival_notifier.moc"

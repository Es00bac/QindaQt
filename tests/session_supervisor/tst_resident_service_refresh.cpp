// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/resident_service_refresh.h"

#include <QProcess>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest>

namespace {

// Fakes just enough of org.freedesktop.systemd1.Manager to observe
// RestartUnit calls without a real systemd user manager or real services.
class FakeUserManager final : public QDBusVirtualObject {
public:
    QStringList requestedUnits;
    QStringList requestedModes;
    QString failingUnit;

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.member() != QStringLiteral("RestartUnit")) return false;
        const QString unit = message.arguments().value(0).toString();
        const QString mode = message.arguments().value(1).toString();
        requestedUnits.append(unit);
        requestedModes.append(mode);
        if (unit == failingUnit) {
            connection.send(message.createErrorReply(
                QStringLiteral("org.freedesktop.systemd1.NoSuchUnit"),
                QStringLiteral("test-forced failure")));
            return true;
        }
        connection.send(message.createReply(
            QVariant::fromValue(QDBusObjectPath(QStringLiteral("/job/1")))));
        return true;
    }
};

} // namespace

class ResidentServiceRefreshTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    // This is the regression the manual physical-session recovery on
    // 2026-09-06 stood in for: a service already resident from a prior
    // desktop keeps its stale Wayland socket after SetEnvironment runs, so
    // the session must explicitly restart it rather than only publish the
    // new environment for future activations.
    void restartsEachConfiguredResidentUnitIndependentlyOfEnvironmentPublication()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeUserManager manager;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager));

        QProcess publisher;
        // No environment values are set here at all: this exercises only the
        // resident-refresh path, proving it is not merely a side effect of
        // republishing the activation environment.
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("qindaqt-test-resident-a.service"),
                         QStringLiteral("qindaqt-test-resident-b.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);

        QCOMPARE(manager.requestedUnits,
                 QStringList({QStringLiteral("qindaqt-test-resident-a.service"),
                              QStringLiteral("qindaqt-test-resident-b.service")}));
        for (const QString &mode : manager.requestedModes) {
            QCOMPARE(mode, QStringLiteral("replace"));
        }
        QVERIFY(!publisher.readAllStandardError().contains("Could not restart"));

        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    // One unit failing to restart (missing, or the job manager rejects it)
    // must not abandon the remaining units in the fixed list, and must not
    // fail the session startup that calls this helper.
    void oneFailingUnitDoesNotStopTheRemainingRefresh()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeUserManager manager;
        manager.failingUnit = QStringLiteral("qindaqt-test-resident-a.service");
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager));

        QProcess publisher;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("qindaqt-test-resident-a.service"),
                         QStringLiteral("qindaqt-test-resident-b.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        // Best-effort: the helper process still exits cleanly even though one
        // restart failed.
        QCOMPARE(publisher.exitCode(), 0);

        QCOMPARE(manager.requestedUnits,
                 QStringList({QStringLiteral("qindaqt-test-resident-a.service"),
                              QStringLiteral("qindaqt-test-resident-b.service")}));
        QVERIFY(publisher.readAllStandardError().contains("Could not restart"));

        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    // The fixed list itself is the reviewed product contract: only modules
    // that independently open a Wayland connection, or that otherwise cache
    // session-scoped desktop identity/routing at their own startup, belong
    // here.
    void residentUnitListNamesOnlyReviewedResidentServices()
    {
        const QStringList units = QindaQt::SessionSupervisor::residentServiceRefreshUnits();
        QCOMPARE(units, QStringList({QStringLiteral("qindaqt-clipboard-host.service"),
                                     QStringLiteral("qindaqt-display-service.service"),
                                     QStringLiteral("xdg-desktop-portal.service"),
                                     QStringLiteral("plasma-xdg-desktop-portal-kde.service")}));
    }
};
QTEST_GUILESS_MAIN(ResidentServiceRefreshTests)
#include "tst_resident_service_refresh.moc"

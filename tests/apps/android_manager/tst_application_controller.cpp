// SPDX-License-Identifier: GPL-3.0-or-later
#include "manager_test_support.h"
#include "qindaqt/shell_launcher/launcher_bounds.h"
using namespace QindaQt::AndroidManager;
using namespace QindaQt::AndroidManager::TestSupport;

class ApplicationControllerTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void missingBackendIsUnavailable();
    void inventorySearchUsesStableRegisteredIdentity();
    void launchUsesExactRevisionAndNeverInventsRunning();
    void refusalAndBusyDoNotReplay();
    void ownerLossRetiresPendingAndLateResult();
    void recipeChangeRetiresPending();
    void invalidSnapshotsPublishNoPartialRows();
    void staleAndChangedSameRevisionFailClosed();
    void synchronousResultIsNotLost();
    void timeoutReportsUncertaintyWithoutReplay();
    void destroyedPortWithdrawsAuthority();
    void runningWithoutWindowCountStaysIndependent();
};

void ApplicationControllerTest::missingBackendIsUnavailable()
{
    ApplicationController controller;
    QCOMPARE(controller.runtimeState(), QStringLiteral("unavailable"));
    QVERIFY(controller.applications().isEmpty());
    QVERIFY(!controller.canRefresh());
    QVERIFY(!controller.refresh());
    QVERIFY(!controller.launch(QStringLiteral("arbitrary")));
}
void ApplicationControllerTest::inventorySearchUsesStableRegisteredIdentity()
{
    FakePort port;
    ApplicationController controller(&port);
    QCOMPARE(controller.runtimeState(), QStringLiteral("ready"));
    QCOMPARE(controller.applications().size(), 2);
    controller.setQuery(QStringLiteral("arithmetic"));
    QCOMPARE(controller.applications().size(), 1);
    QCOMPARE(controller.applications().first().toMap().value(QStringLiteral("entryId")).toString(), entryId(port.current));
    const auto before = controller.query();
    controller.setQuery(QString(controller.queryLimit() + 1, QLatin1Char('x')));
    QCOMPARE(controller.query(), before);
}
void ApplicationControllerTest::launchUsesExactRevisionAndNeverInventsRunning()
{
    FakePort port;
    ApplicationController controller(&port);
    QVERIFY(controller.launch(entryId(port.current)));
    QVERIFY(controller.busy());
    QCOMPARE(port.requests.size(), 1);
    QCOMPARE(port.requests.first().application, port.current.registrations.first().binding);
    QCOMPARE(port.requests.first().attachmentId, port.current.attachmentId);
    QCOMPARE(port.requests.first().snapshotRevision, quint64(1));
    QVERIFY(!port.requests.first().operationId.isEmpty());
    QCOMPARE(controller.applications().first().toMap().value(QStringLiteral("state")).toString(), QStringLiteral("ready"));
    port.result(port.requests.first(), LaunchPhase::Completed);
    QVERIFY(!controller.busy());
    QCOMPARE(controller.applications().first().toMap().value(QStringLiteral("state")).toString(), QStringLiteral("ready"));
    QCOMPARE(port.refreshes, 1);
}
void ApplicationControllerTest::refusalAndBusyDoNotReplay()
{
    FakePort port;
    ApplicationController controller(&port);
    port.accept = false;
    QVERIFY(!controller.launch(entryId(port.current)));
    QVERIFY(!controller.busy());
    QCOMPARE(controller.lastError(), QStringLiteral("Synthetic refusal"));
    port.accept = true;
    QVERIFY(controller.launch(entryId(port.current)));
    QVERIFY(!controller.launch(entryId(port.current, 1)));
    QCOMPARE(port.requests.size(), 2);
    controller.refresh();
    QCOMPARE(port.requests.size(), 2);
}
void ApplicationControllerTest::ownerLossRetiresPendingAndLateResult()
{
    FakePort port;
    ApplicationController controller(&port);
    QVERIFY(controller.launch(entryId(port.current)));
    const auto old = port.requests.first();
    port.publish({});
    QVERIFY(!controller.busy());
    QVERIFY(controller.applications().isEmpty());
    QCOMPARE(controller.runtimeState(), QStringLiteral("unavailable"));
    auto fresh = readySnapshot();
    fresh.attachmentId = QStringLiteral("abcdef01-1234-4234-8234-123456789abc");
    port.publish(fresh);
    QVERIFY(controller.launch(entryId(fresh)));
    port.result(old, LaunchPhase::Completed);
    QVERIFY(controller.busy());
    QCOMPARE(port.requests.size(), 2);
    port.result(port.requests.last(), LaunchPhase::Failed, QStringLiteral("Fresh failure"));
    QVERIFY(!controller.busy());
    QCOMPARE(controller.lastError(), QStringLiteral("Fresh failure"));
}
void ApplicationControllerTest::recipeChangeRetiresPending()
{
    FakePort port;
    ApplicationController controller(&port);
    QVERIFY(controller.launch(entryId(port.current)));
    const auto old = port.requests.first();
    auto changed = port.current;
    changed.revision = 2;
    changed.registrations.first().binding.registrationRevision = 2;
    changed.registrations.first().binding.recipeDigest = QString(64, QLatin1Char('b'));
    changed.applications.first().binding = changed.registrations.first().binding;
    port.publish(changed);
    QVERIFY(!controller.busy());
    QVERIFY(!controller.lastError().isEmpty());
    port.result(old, LaunchPhase::Completed);
    QVERIFY(!controller.busy());
    QCOMPARE(port.requests.size(), 1);
}
void ApplicationControllerTest::invalidSnapshotsPublishNoPartialRows()
{
    FakePort port;
    ApplicationController controller(&port);
    auto invalid = port.current;
    invalid.revision = 2;
    invalid.applications.first().binding.registrationRevision = 999;
    port.publish(invalid);
    QVERIFY(controller.applications().isEmpty());
    QCOMPARE(controller.runtimeState(), QStringLiteral("unavailable"));
    invalid = readySnapshot(); invalid.registrations.first().binding.origin = QindaQt::ShellLauncher::ApplicationOrigin::Windows;
    invalid.applications.first().binding = invalid.registrations.first().binding;
    QVERIFY(!validateRuntimeSnapshot(invalid));
    invalid = readySnapshot(); invalid.applications.first().state = static_cast<ApplicationState>(99);
    QVERIFY(!validateRuntimeSnapshot(invalid));
    invalid = readySnapshot(); invalid.applications.first().windowCount = 4097;
    QVERIFY(!validateRuntimeSnapshot(invalid));
    invalid = readySnapshot(); invalid.message = QStringLiteral("bad\nmessage");
    QVERIFY(!validateRuntimeSnapshot(invalid));
}
void ApplicationControllerTest::staleAndChangedSameRevisionFailClosed()
{
    FakePort port;
    port.current.revision = 2;
    ApplicationController controller(&port);
    auto stale = port.current; stale.revision = 1;
    port.publish(stale);
    QVERIFY(controller.applications().isEmpty());
    auto fresh = readySnapshot(); fresh.revision = 3;
    port.publish(fresh);
    QCOMPARE(controller.applications().size(), 2);
    fresh.message = QStringLiteral("Changed without revision");
    port.publish(fresh);
    QVERIFY(controller.applications().isEmpty());
}
void ApplicationControllerTest::synchronousResultIsNotLost()
{
    FakePort port;
    port.finishInline = true;
    ApplicationController controller(&port);
    QVERIFY(controller.launch(entryId(port.current)));
    QVERIFY(!controller.busy());
    QCOMPARE(port.requests.size(), 1);
}
void ApplicationControllerTest::timeoutReportsUncertaintyWithoutReplay()
{
    FakePort port;
    ApplicationController controller(&port, nullptr, 2);
    QVERIFY(controller.launch(entryId(port.current)));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 100);
    QVERIFY(controller.lastError().contains(QStringLiteral("could not be confirmed")));
    QCOMPARE(port.requests.size(), 1);
    port.result(port.requests.first(), LaunchPhase::Completed);
    QCOMPARE(port.requests.size(), 1);
}
void ApplicationControllerTest::destroyedPortWithdrawsAuthority()
{
    auto *port = new FakePort;
    ApplicationController controller(port);
    QVERIFY(controller.launch(entryId(port->current)));
    delete port;
    QVERIFY(!controller.canRefresh());
    QVERIFY(!controller.busy());
    QVERIFY(controller.applications().isEmpty());
}
void ApplicationControllerTest::runningWithoutWindowCountStaysIndependent()
{
    FakePort port;
    port.current.applications.first().state = ApplicationState::Running;
    ApplicationController controller(&port);
    const auto row = controller.applications().first().toMap();
    QCOMPARE(row.value(QStringLiteral("state")).toString(), QStringLiteral("running"));
    QVERIFY(!row.value(QStringLiteral("hasWindowCount")).toBool());
}
QTEST_GUILESS_MAIN(ApplicationControllerTest)
#include "tst_application_controller.moc"

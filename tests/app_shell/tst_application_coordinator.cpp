// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/app_shell/application_coordinator.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::AppShell;

class ApplicationCoordinatorTest final : public QObject {
    Q_OBJECT

private slots:
    void applicationOwnsQuitDecision();
    void projectsOnlyConfirmedIntegrationState();
    void serializesAndFencesPortalResults();
    void rejectsHostilePortalInputsWithoutPublishing();
    void forwardsKnownEnabledActions();
    void boundsFocusIdentity();
};

void ApplicationCoordinatorTest::applicationOwnsQuitDecision()
{
    ApplicationCoordinator coordinator;
    QSignalSpy decision(&coordinator, &ApplicationCoordinator::quitDecisionRequested);
    QSignalSpy approved(&coordinator, &ApplicationCoordinator::quitApproved);
    QSignalSpy rejected(&coordinator, &ApplicationCoordinator::quitRejected);

    const quint64 first = coordinator.requestQuit(QStringLiteral("window-close"));
    QVERIFY(first != 0);
    QVERIFY(coordinator.quitPending());
    QCOMPARE(decision.count(), 1);
    QCOMPARE(coordinator.requestQuit(QStringLiteral("shortcut")), quint64(0));
    QCOMPARE(coordinator.lastErrorCode(), ErrorCode::Busy);

    QCOMPARE(coordinator.resolveQuit(first + 1, true).code, ErrorCode::StaleRequest);
    QVERIFY(coordinator.quitPending());
    QCOMPARE(coordinator.resolveQuit(first, false, QStringLiteral("Unsaved work")).code,
             ErrorCode::None);
    QVERIFY(!coordinator.quitPending());
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(approved.count(), 0);

    const quint64 second = coordinator.requestQuit(QStringLiteral("shortcut"));
    QCOMPARE(coordinator.resolveQuit(second, true).code, ErrorCode::None);
    QCOMPARE(approved.count(), 1);
}

void ApplicationCoordinatorTest::projectsOnlyConfirmedIntegrationState()
{
    ApplicationCoordinator coordinator;
    QVERIFY(!coordinator.degraded());
    coordinator.setSettingsState(IntegrationState::Unavailable,
                                 QStringLiteral("Preferences remain local."));
    QVERIFY(coordinator.degraded());
    QVERIFY(coordinator.degradedMessage().contains(QStringLiteral("Settings")));
    coordinator.setSessionState(IntegrationState::Degraded,
                                QStringLiteral("Restore is delayed."));
    QVERIFY(coordinator.degradedMessage().contains(QStringLiteral("Session")));
    coordinator.setSettingsState(IntegrationState::Ready);
    QVERIFY(!coordinator.degradedMessage().contains(QStringLiteral("Settings")));
    coordinator.setSessionState(IntegrationState::NotRequired);
    QVERIFY(!coordinator.degraded());
}

void ApplicationCoordinatorTest::serializesAndFencesPortalResults()
{
    ApplicationCoordinator coordinator;
    QSignalSpy issued(&coordinator, &ApplicationCoordinator::portalRequestIssued);
    QSignalSpy finished(&coordinator, &ApplicationCoordinator::portalFinished);

    const quint64 request = coordinator.requestOpenFile(
        QStringLiteral("Open document"), {QStringLiteral("text/plain")});
    QVERIFY(request != 0);
    QCOMPARE(issued.count(), 1);
    QCOMPARE(coordinator.requestFolder(QStringLiteral("Choose folder")), quint64(0));
    QCOMPARE(coordinator.lastErrorCode(), ErrorCode::Busy);
    QCOMPARE(coordinator.resolvePortal(request + 1, true,
                                       {QUrl(QStringLiteral("file:///tmp/wrong"))})
                 .code,
             ErrorCode::StaleRequest);
    QCOMPARE(finished.count(), 0);
    QCOMPARE(coordinator.resolvePortal(request, true,
                                       {QUrl(QStringLiteral("file:///tmp/document.txt"))})
                 .code,
             ErrorCode::None);
    QCOMPARE(finished.count(), 1);
}

void ApplicationCoordinatorTest::rejectsHostilePortalInputsWithoutPublishing()
{
    ApplicationCoordinator coordinator;
    QSignalSpy issued(&coordinator, &ApplicationCoordinator::portalRequestIssued);
    QCOMPARE(coordinator.requestOpenFile(QStringLiteral("Open"),
                                         {QStringLiteral("not a mime type")}),
             quint64(0));
    QCOMPARE(coordinator.lastErrorCode(), ErrorCode::InvalidArgument);
    QCOMPARE(issued.count(), 0);

    QCOMPARE(coordinator.requestSaveFile(QStringLiteral("Save"),
                                         QStringLiteral("../escape"),
                                         {QStringLiteral("text/plain")}),
             quint64(0));
    QCOMPARE(issued.count(), 0);

    QCOMPARE(coordinator.requestOpenFile(QStringLiteral(" Open "),
                                         {QStringLiteral("text/plain")}),
             quint64(0));
    QCOMPARE(issued.count(), 0);
}

void ApplicationCoordinatorTest::forwardsKnownEnabledActions()
{
    ApplicationCoordinator coordinator;
    const ActionSpec action{.id = QStringLiteral("file.quit"),
                            .menuId = QStringLiteral("file"),
                            .menuLabel = QStringLiteral("File"),
                            .label = QStringLiteral("Quit"),
                            .accessibleDescription = QStringLiteral("Close this window"),
                            .shortcut = QKeySequence(QKeySequence::Quit)};
    QCOMPARE(coordinator.replaceActions({action}).code, ErrorCode::None);
    QSignalSpy requested(&coordinator, &ApplicationCoordinator::actionRequested);
    QVERIFY(coordinator.activateAction(QStringLiteral("file.quit")));
    QCOMPARE(requested.count(), 1);
    QVERIFY(!coordinator.activateAction(QStringLiteral("file.unknown")));
    QCOMPARE(coordinator.lastErrorCode(), ErrorCode::UnknownAction);
}

void ApplicationCoordinatorTest::boundsFocusIdentity()
{
    ApplicationCoordinator coordinator;
    coordinator.setInitialFocusObjectName(QStringLiteral("documentView"));
    QCOMPARE(coordinator.initialFocusObjectName(), QStringLiteral("documentView"));
    coordinator.reportFocusOwner(QStringLiteral("documentView"));
    QCOMPARE(coordinator.focusOwnerObjectName(), QStringLiteral("documentView"));
    coordinator.reportFocusOwner(QStringLiteral("bad focus name"));
    QCOMPARE(coordinator.focusOwnerObjectName(), QStringLiteral("documentView"));
    QCOMPARE(coordinator.lastErrorCode(), ErrorCode::InvalidArgument);
}

// Standard Qt key sequences consult the GUI platform theme. The coordinator
// remains GUI-thread owned, so exercise it with the same application class as
// a real first-party QML client instead of an invalid QCoreApplication setup.
QTEST_MAIN(ApplicationCoordinatorTest)
#include "tst_application_coordinator.moc"

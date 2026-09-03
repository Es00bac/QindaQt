// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_logind_service.h"
#include "support/private_bus.h"

#include <qindaqt/services/power_service/adapters/logind_action_authority.h>

#include <QtDBus/QDBusConnection>
#include <QtTest>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

struct ActionRow
{
    std::unique_ptr<PrivateBus> bus;
    std::unique_ptr<FakeLogindService> fake;
    QDBusConnection fakeConnection{QStringLiteral("invalid-fake")};
    std::unique_ptr<Upstream::LogindActionAuthority> authority;
    QDBusConnection clientConnection{QStringLiteral("invalid-client")};
    quint64 generation = 0;

    bool start(const QString &canPowerOff = QStringLiteral("yes"),
               const QString &canReboot = QStringLiteral("no"),
               const QString &canSuspend = QStringLiteral("challenge"),
               const QString &canHibernate = QStringLiteral("na"))
    {
        bus = std::make_unique<PrivateBus>();
        if (!bus->start()) {
            return false;
        }
        fakeConnection = bus->openConnection(QStringLiteral("fake"));
        fake = std::make_unique<FakeLogindService>(fakeConnection);
        if (!fake->registerService()) {
            return false;
        }
        fake->setCanAnswers(canPowerOff, canReboot, canSuspend, canHibernate);
        clientConnection = bus->connection;
        authority = std::make_unique<Upstream::LogindActionAuthority>(clientConnection);
        generation = authority->start();
        return true;
    }
};

} // namespace

class PowerLogindActionTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void admittedSetComesOnlyFromYesAnswers();
    void admittedActionsChangeSignalFires();
    void admittedActionExecutesWithInteractiveFalse();
    void unadmittedActionsNeverReachLogind();
    void cachedYesThatFlipsToNoNeverReachesLogind();
    void challengeIsNotAdmittedWithoutPolkitUi();
    void errorReplyCompletesFailed();
    void ownerReplacementMidFlightCompletesUncertain();
    void duplicateOperationIdDoesNotRedispatchOrDoubleComplete();
    void staleAuthorizationReplyCannotEraseRestartedOperation();
    void stoppedAuthorityRefusesActions();
    void absentAuthorityAdmitsNothing();
};

void PowerLogindActionTests::admittedSetComesOnlyFromYesAnswers()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    const Upstream::AdmittedActions admitted = row.authority->admittedActions();
    QVERIFY(admitted.powerOff);
    QVERIFY(!admitted.reboot);
    QVERIFY(!admitted.suspend);
    QVERIFY(!admitted.hibernate);
}

void PowerLogindActionTests::admittedActionsChangeSignalFires()
{
    ActionRow row;
    QVERIFY(row.start(QStringLiteral("no"), QStringLiteral("no"),
                      QStringLiteral("no"), QStringLiteral("no")));
    QSignalSpy changed(row.authority.get(),
                       &Upstream::LogindActionAuthority::admittedActionsChanged);
    QTRY_COMPARE(changed.size(), 1);
    QVERIFY(!row.authority->admittedActions().powerOff);

    row.fake->setCanAnswers(QStringLiteral("yes"), QStringLiteral("no"),
                            QStringLiteral("no"), QStringLiteral("no"));
    row.authority->refreshAdmittedActions();
    QTRY_COMPARE(changed.size(), 2);
    QVERIFY(row.authority->admittedActions().powerOff);
}

void PowerLogindActionTests::admittedActionExecutesWithInteractiveFalse()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(11, Upstream::SessionAction::PowerOff);
    QTRY_COMPARE(finished.size(), 1);
    const CollaboratorOutcome outcome = finished.first().at(2).value<CollaboratorOutcome>();
    QCOMPARE(outcome.status, CollaboratorStatus::Succeeded);
    QCOMPARE(outcome.reasonCode, QStringLiteral("applied"));
    QCOMPARE(row.fake->actionCalls.size(), 1);
    QCOMPARE(row.fake->actionCalls.constFirst().method, QStringLiteral("PowerOff"));
    // QindaQt never passes interactive=true: there is no polkit UI on this path.
    QVERIFY(!row.fake->actionCalls.constFirst().interactive);
}

void PowerLogindActionTests::unadmittedActionsNeverReachLogind()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(12, Upstream::SessionAction::Reboot);
    QTRY_COMPARE(finished.size(), 1);
    const CollaboratorOutcome outcome = finished.first().at(2).value<CollaboratorOutcome>();
    QCOMPARE(outcome.status, CollaboratorStatus::Unsupported);
    QCOMPARE(outcome.reasonCode, QStringLiteral("action-not-admitted"));
    QCOMPARE(row.fake->actionCalls.size(), 0);
}

void PowerLogindActionTests::cachedYesThatFlipsToNoNeverReachesLogind()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);

    // Do not refresh the presentation cache. Dispatch itself must consult the
    // current caller-relative logind answer and reject this stale historical
    // "yes" before any PowerOff call is sent.
    row.fake->setCanAnswers(QStringLiteral("no"), QStringLiteral("no"),
                            QStringLiteral("challenge"), QStringLiteral("na"));
    QVERIFY(row.authority->admittedActions().powerOff);
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(19, Upstream::SessionAction::PowerOff);
    QTRY_COMPARE(finished.size(), 1);
    const CollaboratorOutcome outcome =
        finished.constFirst().at(2).value<CollaboratorOutcome>();
    QCOMPARE(outcome.status, CollaboratorStatus::Unsupported);
    QCOMPARE(outcome.reasonCode, QStringLiteral("action-not-admitted"));
    QCOMPARE(row.fake->actionCalls.size(), 0);
}

void PowerLogindActionTests::challengeIsNotAdmittedWithoutPolkitUi()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(13, Upstream::SessionAction::Suspend);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.first().at(2).value<CollaboratorOutcome>().status,
             CollaboratorStatus::Unsupported);
    QCOMPARE(row.fake->actionCalls.size(), 0);
}

void PowerLogindActionTests::errorReplyCompletesFailed()
{
    ActionRow row;
    QVERIFY(row.start(QStringLiteral("yes"), QStringLiteral("yes"),
                      QStringLiteral("yes"), QStringLiteral("yes")));
    row.fake->setFailHibernate(true);
    QTRY_VERIFY(row.authority->admittedActions().hibernate);
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(14, Upstream::SessionAction::Hibernate);
    QTRY_COMPARE(finished.size(), 1);
    const CollaboratorOutcome outcome = finished.first().at(2).value<CollaboratorOutcome>();
    QCOMPARE(outcome.status, CollaboratorStatus::Failed);
    QCOMPARE(outcome.reasonCode, QStringLiteral("action-rejected"));
}

void PowerLogindActionTests::ownerReplacementMidFlightCompletesUncertain()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    // Defer the upstream reply so the owner can be replaced while the action
    // is in flight; a dispatch under a replaced authority can never be
    // reported as a plain success.
    row.fake->setDeferNextActionReply(true);
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(15, Upstream::SessionAction::PowerOff);
    QTRY_COMPARE(row.fake->actionCalls.size(), 1);
    QCOMPARE(finished.size(), 0);

    // The deferred reply stays on the old connection, which must remain open
    // long enough for the stale authority to answer.
    row.fake->unregisterService();
    const QDBusConnection replacementConnection =
        row.bus->openConnection(QStringLiteral("replacement"));
    auto replacement = std::make_unique<FakeLogindService>(replacementConnection);
    QVERIFY(replacement->registerService());
    row.fake->completeDeferredActionReply();
    QTRY_COMPARE(finished.size(), 1);
    const CollaboratorOutcome outcome = finished.first().at(2).value<CollaboratorOutcome>();
    QCOMPARE(outcome.status, CollaboratorStatus::Uncertain);
    QCOMPARE(outcome.reasonCode, QStringLiteral("authority-replaced"));
}

void PowerLogindActionTests::duplicateOperationIdDoesNotRedispatchOrDoubleComplete()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    row.fake->setDeferNextActionReply(true);
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(18, Upstream::SessionAction::PowerOff);
    row.authority->submitAction(18, Upstream::SessionAction::PowerOff);
    QTRY_COMPARE(row.fake->actionCalls.size(), 1);
    row.fake->completeDeferredActionReply();
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.constFirst().at(1).toULongLong(), quint64(18));
}

void PowerLogindActionTests::staleAuthorizationReplyCannotEraseRestartedOperation()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    const quint64 firstGeneration = row.generation;
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);

    row.fake->setDeferNextCanPowerOffReply(true);
    row.authority->submitAction(77, Upstream::SessionAction::PowerOff);
    QTRY_COMPARE(row.fake->deferredCanPowerOffReplyCount(), qsizetype(1));

    row.authority->stop();
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.constFirst().at(0).toULongLong(), firstGeneration);
    QCOMPARE(finished.constFirst().at(1).toULongLong(), quint64(77));
    QCOMPARE(finished.constFirst().at(2).value<CollaboratorOutcome>().status,
             CollaboratorStatus::Uncertain);

    const quint64 secondGeneration = row.authority->start();
    QVERIFY(secondGeneration != firstGeneration);
    row.fake->setDeferNextCanPowerOffReply(true);
    row.authority->submitAction(77, Upstream::SessionAction::PowerOff);
    QTRY_COMPARE(row.fake->deferredCanPowerOffReplyCount(), qsizetype(2));
    QTRY_VERIFY(row.authority->admittedActions().powerOff);

    // The old generation must be observationally inert: it cannot consume the
    // current run's pending entry even though both operations use ID 77.
    row.fake->completeOldestDeferredCanPowerOffReply();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(row.fake->actionCalls.size(), 0);

    row.fake->completeOldestDeferredCanPowerOffReply();
    QTRY_COMPARE(finished.size(), 2);
    QCOMPARE(finished.constLast().at(0).toULongLong(), secondGeneration);
    QCOMPARE(finished.constLast().at(1).toULongLong(), quint64(77));
    QCOMPARE(finished.constLast().at(2).value<CollaboratorOutcome>().status,
             CollaboratorStatus::Succeeded);
    QCOMPARE(row.fake->actionCalls.size(), 1);
}

void PowerLogindActionTests::stoppedAuthorityRefusesActions()
{
    ActionRow row;
    QVERIFY(row.start());
    QTRY_VERIFY(row.authority->admittedActions().powerOff);
    row.authority->stop();
    QSignalSpy finished(row.authority.get(),
                        &Upstream::LogindActionAuthority::actionFinished);
    row.authority->submitAction(16, Upstream::SessionAction::PowerOff);
    QCOMPARE(finished.size(), 1);
    const CollaboratorOutcome outcome = finished.first().at(2).value<CollaboratorOutcome>();
    QCOMPARE(outcome.status, CollaboratorStatus::Failed);
    QCOMPARE(outcome.reasonCode, QStringLiteral("logind-unavailable"));
    QCOMPARE(row.fake->actionCalls.size(), 0);
}

void PowerLogindActionTests::absentAuthorityAdmitsNothing()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    Upstream::LogindActionAuthority authority(bus.connection);
    QSignalSpy changed(&authority, &Upstream::LogindActionAuthority::admittedActionsChanged);
    authority.start();
    QTRY_COMPARE(changed.size(), 1);
    const Upstream::AdmittedActions admitted = authority.admittedActions();
    QVERIFY(!admitted.powerOff && !admitted.reboot && !admitted.suspend
            && !admitted.hibernate);
    QSignalSpy finished(&authority, &Upstream::LogindActionAuthority::actionFinished);
    authority.submitAction(17, Upstream::SessionAction::PowerOff);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.first().at(2).value<CollaboratorOutcome>().status,
             CollaboratorStatus::Unsupported);
}

QTEST_GUILESS_MAIN(PowerLogindActionTests)
#include "tst_power_logind_actions.moc"

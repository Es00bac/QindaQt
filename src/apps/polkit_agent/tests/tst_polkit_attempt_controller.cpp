// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_attempt_controller.h"

#include <QtTest>

using namespace QindaQt::Apps::PolkitAgent;

namespace {

class FakeAttempt final : public AuthenticationAttempt {
public:
    QStringList responses;
    int cancelCount = 0;

    void respond(const QString &response) override { responses.append(response); }
    void cancel() override { ++cancelCount; }
};

QList<AgentIdentity> oneIdentity()
{
    return {{QStringLiteral("Jarrod C (jarrod)"), QStringLiteral("unix-user:1000"), true}};
}

} // namespace

class PolkitAttemptControllerTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void wrongPasswordStartsANewAttemptAndDoesNotComplete();
    void correctPasswordCompletesWithAuthorization();
    void userCancelCompletesWithoutRetrying();
    void externalCancelFromPolkitCompletesWithoutRetrying();
    void cancelBeforeStartCompletesImmediately();
};

void PolkitAttemptControllerTest::wrongPasswordStartsANewAttemptAndDoesNotComplete()
{
    QList<FakeAttempt *> created;
    PolkitAttemptController controller(oneIdentity(), 0, [&created](const QString &) {
        auto attempt = std::make_unique<FakeAttempt>();
        created.append(attempt.get());
        return attempt;
    });
    QSignalSpy attemptFailedSpy(&controller, &PolkitAttemptController::attemptFailed);
    QSignalSpy completedSpy(&controller, &PolkitAttemptController::completed);
    controller.start();
    QCOMPARE(created.size(), 1);
    controller.authenticate(QStringLiteral("wrong"));
    QCOMPARE(created.constFirst()->responses, QStringList{QStringLiteral("wrong")});
    created.constFirst()->completed(false);
    QCOMPARE(attemptFailedSpy.count(), 1);
    QCOMPARE(attemptFailedSpy.constFirst().constFirst().toString(),
             QStringLiteral("That password didn't work. Try again."));
    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(created.size(), 2);
}

void PolkitAttemptControllerTest::correctPasswordCompletesWithAuthorization()
{
    QList<FakeAttempt *> created;
    PolkitAttemptController controller(oneIdentity(), 0, [&created](const QString &) {
        auto attempt = std::make_unique<FakeAttempt>();
        created.append(attempt.get());
        return attempt;
    });
    QSignalSpy completedSpy(&controller, &PolkitAttemptController::completed);
    controller.start();
    created.constFirst()->completed(true);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.constFirst().constFirst().toBool(), true);
    QCOMPARE(created.size(), 1);
}

void PolkitAttemptControllerTest::userCancelCompletesWithoutRetrying()
{
    QList<FakeAttempt *> created;
    PolkitAttemptController controller(oneIdentity(), 0, [&created](const QString &) {
        auto attempt = std::make_unique<FakeAttempt>();
        created.append(attempt.get());
        return attempt;
    });
    QSignalSpy completedSpy(&controller, &PolkitAttemptController::completed);
    QSignalSpy attemptFailedSpy(&controller, &PolkitAttemptController::attemptFailed);
    controller.start();
    controller.cancel();
    QCOMPARE(created.constFirst()->cancelCount, 1);
    // PolkitQt1::Agent::Session::cancel() is documented to emit completed()
    // asynchronously; the fake mirrors that shape.
    created.constFirst()->completed(false);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.constFirst().constFirst().toBool(), false);
    QCOMPARE(attemptFailedSpy.count(), 0);
    QCOMPARE(created.size(), 1);
}

void PolkitAttemptControllerTest::externalCancelFromPolkitCompletesWithoutRetrying()
{
    QList<FakeAttempt *> created;
    PolkitAttemptController controller(oneIdentity(), 0, [&created](const QString &) {
        auto attempt = std::make_unique<FakeAttempt>();
        created.append(attempt.get());
        return attempt;
    });
    QSignalSpy completedSpy(&controller, &PolkitAttemptController::completed);
    controller.start();
    controller.cancelExternally();
    QCOMPARE(created.constFirst()->cancelCount, 1);
    created.constFirst()->completed(false);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.constFirst().constFirst().toBool(), false);
    QCOMPARE(created.size(), 1);
}

void PolkitAttemptControllerTest::cancelBeforeStartCompletesImmediately()
{
    int factoryCalls = 0;
    PolkitAttemptController controller(oneIdentity(), 0, [&factoryCalls](const QString &) {
        ++factoryCalls;
        return std::make_unique<FakeAttempt>();
    });
    QSignalSpy completedSpy(&controller, &PolkitAttemptController::completed);
    controller.cancel();
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.constFirst().constFirst().toBool(), false);
    QCOMPARE(factoryCalls, 0);
    controller.start();
    QCOMPARE(factoryCalls, 0);
}

QTEST_MAIN(PolkitAttemptControllerTest)
#include "tst_polkit_attempt_controller.moc"

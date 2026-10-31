// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_attempt_controller.h"

#include <QPointer>
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

// Behaves like polkit-qt's Session::Private::completed(): emits completed()
// and then keeps using itself. `destroyedWhileEmitting` records whether the
// listener chain destroyed it inside that emission (the 2026-10-31 crash).
class SelfTouchingAttempt final : public AuthenticationAttempt {
public:
    bool destroyedWhileEmitting = false;
    void respond(const QString &) override { }
    void cancel() override { }
    // Returns false when this object was destroyed during the emission.
    static bool finish(SelfTouchingAttempt *attempt, bool gained)
    {
        QPointer<SelfTouchingAttempt> self(attempt);
        Q_EMIT attempt->completed(gained);
        return !self.isNull();
    }
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
    void failedAttemptOutlivesItsOwnCompletedSignal();
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

void PolkitAttemptControllerTest::failedAttemptOutlivesItsOwnCompletedSignal()
{
    QList<QPointer<SelfTouchingAttempt>> created;
    PolkitAttemptController controller(oneIdentity(), 0, [&created](const QString &) {
        auto attempt = std::make_unique<SelfTouchingAttempt>();
        created.append(attempt.get());
        return attempt;
    });
    controller.start();
    // A wrong password: the retry must not free the attempt that is emitting.
    QVERIFY(SelfTouchingAttempt::finish(created.constFirst(), false));
    QCOMPARE(created.size(), 2);
    // ...and it is still released once the event loop runs.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(created.constFirst().isNull());
    QVERIFY(!created.at(1).isNull());
}

QTEST_MAIN(PolkitAttemptControllerTest)
#include "tst_polkit_attempt_controller.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_request_queue.h"

#include <QPointer>
#include <QtTest>

using namespace QindaQt::Apps::PolkitAgent;

namespace {

class FakeAttempt final : public AuthenticationAttempt {
public:
    int cancelCount = 0;
    void respond(const QString &) override { }
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

AuthenticationRequest makeRequest(const QString &cookie)
{
    AuthenticationRequest request;
    request.cookie = cookie;
    request.message = QStringLiteral("Authenticate for %1").arg(cookie);
    request.identities = {
        {QStringLiteral("Jarrod C (jarrod)"), QStringLiteral("unix-user:1000"), true},
    };
    return request;
}

} // namespace

class PolkitRequestQueueTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void secondRequestQueuesWhileFirstIsActive();
    void cancelActivePromotesTheQueuedRequest();
    void completedRequestOutlivesItsOwnCompletedSignal();
};

void PolkitRequestQueueTest::secondRequestQueuesWhileFirstIsActive()
{
    QList<FakeAttempt *> created;
    PolkitRequestQueue queue([&created](const AuthenticationRequest &request) {
        return std::make_unique<PolkitAttemptController>(
            request.identities, 0, [&created](const QString &) {
                auto attempt = std::make_unique<FakeAttempt>();
                created.append(attempt.get());
                return attempt;
            });
    });
    QSignalSpy activatedSpy(&queue, &PolkitRequestQueue::requestActivated);
    QSignalSpy idleSpy(&queue, &PolkitRequestQueue::queueIdle);

    queue.enqueue(makeRequest(QStringLiteral("cookie-a")));
    QCOMPARE(activatedSpy.count(), 1);
    QVERIFY(queue.activeRequest() != nullptr);
    QCOMPARE(queue.activeRequest()->cookie, QStringLiteral("cookie-a"));
    QCOMPARE(queue.pendingCount(), 0);
    QCOMPARE(created.size(), 1);

    queue.enqueue(makeRequest(QStringLiteral("cookie-b")));
    // Queued, not dropped and not activated yet: the open dialog is untouched.
    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(queue.pendingCount(), 1);
    QCOMPARE(queue.activeRequest()->cookie, QStringLiteral("cookie-a"));
    QCOMPARE(created.size(), 1);

    created.constFirst()->completed(true);
    QCOMPARE(activatedSpy.count(), 2);
    QVERIFY(queue.activeRequest() != nullptr);
    QCOMPARE(queue.activeRequest()->cookie, QStringLiteral("cookie-b"));
    QCOMPARE(queue.pendingCount(), 0);
    QCOMPARE(created.size(), 2);
    QCOMPARE(idleSpy.count(), 0);

    created.at(1)->completed(true);
    QCOMPARE(idleSpy.count(), 1);
    QVERIFY(!queue.hasActive());
}

void PolkitRequestQueueTest::cancelActivePromotesTheQueuedRequest()
{
    QList<FakeAttempt *> created;
    PolkitRequestQueue queue([&created](const AuthenticationRequest &request) {
        return std::make_unique<PolkitAttemptController>(
            request.identities, 0, [&created](const QString &) {
                auto attempt = std::make_unique<FakeAttempt>();
                created.append(attempt.get());
                return attempt;
            });
    });
    queue.enqueue(makeRequest(QStringLiteral("cookie-a")));
    queue.enqueue(makeRequest(QStringLiteral("cookie-b")));
    queue.cancelActive();
    QCOMPARE(created.constFirst()->cancelCount, 1);
    created.constFirst()->completed(false);
    QVERIFY(queue.activeRequest() != nullptr);
    QCOMPARE(queue.activeRequest()->cookie, QStringLiteral("cookie-b"));
}

void PolkitRequestQueueTest::completedRequestOutlivesItsOwnCompletedSignal()
{
    QList<QPointer<SelfTouchingAttempt>> created;
    QPointer<PolkitAttemptController> controller;
    PolkitRequestQueue queue([&created, &controller](const AuthenticationRequest &request) {
        auto made = std::make_unique<PolkitAttemptController>(
            request.identities, 0, [&created](const QString &) {
                auto attempt = std::make_unique<SelfTouchingAttempt>();
                created.append(attempt.get());
                return attempt;
            });
        controller = made.get();
        return made;
    });
    QSignalSpy idleSpy(&queue, &PolkitRequestQueue::queueIdle);
    queue.enqueue(makeRequest(QStringLiteral("cookie-a")));

    // The right password: the queue retires the controller and its attempt,
    // but not while the attempt is still inside completed().
    QVERIFY(SelfTouchingAttempt::finish(created.constFirst(), true));
    QCOMPARE(idleSpy.count(), 1);
    QVERIFY(!queue.hasActive());
    QVERIFY(!controller.isNull());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(controller.isNull());
    QVERIFY(created.constFirst().isNull());
}

QTEST_MAIN(PolkitRequestQueueTest)
#include "tst_polkit_request_queue.moc"

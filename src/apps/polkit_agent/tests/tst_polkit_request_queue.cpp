// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_request_queue.h"

#include <QtTest>

using namespace QindaQt::Apps::PolkitAgent;

namespace {

class FakeAttempt final : public AuthenticationAttempt {
public:
    int cancelCount = 0;
    void respond(const QString &) override { }
    void cancel() override { ++cancelCount; }
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

QTEST_MAIN(PolkitRequestQueueTest)
#include "tst_polkit_request_queue.moc"

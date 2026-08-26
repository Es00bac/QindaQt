// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_visibility_client/compositor_visibility_client.h"
#include "qindaqt/shell_visibility_client/compositor_visibility_transport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt;

namespace {

struct RecordedRequest {
    quint64 token = 0;
    QString owner;
};

class FakeTransport final
    : public ShellVisibilityClient::CompositorVisibilityTransport {
public:
    using CompositorVisibilityTransport::CompositorVisibilityTransport;

    bool start(QString *error) override
    {
        ++startCalls;
        if (!startSucceeds) {
            if (error) {
                *error = QStringLiteral("injected start failure");
            }
            return false;
        }
        running = true;
        if (error) {
            error->clear();
        }
        return true;
    }

    void stop() override
    {
        ++stopCalls;
        running = false;
    }

    void requestSnapshot(quint64 token, const QString &uniqueOwner) override
    {
        requests.append({token, uniqueOwner});
    }

    void announceOwner(const QString &owner)
    {
        Q_EMIT serviceOwnerChanged(owner);
    }

    void invalidate(const QString &owner)
    {
        Q_EMIT snapshotInvalidated(owner);
    }

    void reply(const RecordedRequest &request, const QByteArray &payload)
    {
        Q_EMIT snapshotReceived(request.token, request.owner, payload);
    }

    void fail(const RecordedRequest &request, const QString &message)
    {
        Q_EMIT snapshotFailed(request.token, request.owner, message);
    }

    QVector<RecordedRequest> requests;
    int startCalls = 0;
    int stopCalls = 0;
    bool startSucceeds = true;
    bool running = false;
};

QByteArray snapshotPayload(QString epoch, quint64 revision, int x = 10)
{
    const auto rectangle = [](int left, int top, int width, int height) {
        return QJsonObject{{QStringLiteral("x"), left},
                           {QStringLiteral("y"), top},
                           {QStringLiteral("width"), width},
                           {QStringLiteral("height"), height}};
    };
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("status"), QStringLiteral("ok")},
                             {QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("epoch"), std::move(epoch)},
                             {QStringLiteral("revision"), QString::number(revision)},
                             {QStringLiteral("scope"),
                              QJsonObject{{QStringLiteral("workspaceId"),
                                           QStringLiteral("workspace-1")},
                                          {QStringLiteral("activityId"),
                                           QStringLiteral("activity-1")}}},
                             {QStringLiteral("outputs"),
                              QJsonArray{QJsonObject{
                                  {QStringLiteral("id"), QStringLiteral("DP-1")},
                                  {QStringLiteral("geometry"),
                                   rectangle(0, 0, 1920, 1080)},
                                  {QStringLiteral("scale"), 1.0}}}},
                             {QStringLiteral("windows"),
                              QJsonArray{QJsonObject{
                                  {QStringLiteral("id"), QStringLiteral("window-1")},
                                  {QStringLiteral("outputId"), QStringLiteral("DP-1")},
                                  {QStringLiteral("frameGeometry"),
                                   rectangle(x, 20, 800, 600)},
                                  {QStringLiteral("workspaceIds"),
                                   QJsonArray{QStringLiteral("workspace-1")}},
                                  {QStringLiteral("onAllWorkspaces"), false},
                                  {QStringLiteral("activityIds"), QJsonArray{}},
                                  {QStringLiteral("active"), true},
                                  {QStringLiteral("maximized"), false},
                                  {QStringLiteral("minimized"), false},
                                  {QStringLiteral("hidden"), false}}}},
                         })
        .toJson(QJsonDocument::Compact);
}

ShellVisibilityClient::CompositorVisibilityClientTiming fastTiming()
{
    return {.debounceMilliseconds = 2,
            .requestTimeoutMilliseconds = 30,
            .retryMilliseconds = {3, 6, 12}};
}

} // namespace

class CompositorVisibilityClientTests final : public QObject {
    Q_OBJECT

private slots:
    void startsWithSafeFallbackAndAcceptsTheInitialOwnerSnapshot();
    void coalescesBurstsAndAlwaysFollowsAnInFlightInvalidation();
    void ignoresLateRepliesAcrossOwnerChanges();
    void timesOutAndRetriesWithoutParallelRequests();
    void malformedPayloadFallsBackThenRecoversFromTheRetainedRevision();
    void serviceLossForcesSafeVisibleAndCancelsPendingWork();
    void staleOwnerInvalidationsAreIgnored();
    void rejectsInvalidTimingAndTransportStartup();
};

void CompositorVisibilityClientTests::
    startsWithSafeFallbackAndAcceptsTheInitialOwnerSnapshot()
{
    FakeTransport transport;
    ShellVisibilityClient::CompositorVisibilityClient client(transport, fastTiming());
    QSignalSpy changed(&client,
                       &ShellVisibilityClient::CompositorVisibilityClient::stateChanged);
    QVERIFY(client.start());
    QVERIFY(client.safeVisibleRequired());

    transport.announceOwner(QStringLiteral(":1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    QVERIFY(client.requestInFlight());
    transport.reply(transport.requests.constFirst(),
                    snapshotPayload(QStringLiteral("epoch-a"), 1));

    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);
    QVERIFY(!client.safeVisibleRequired());
    QCOMPARE(client.snapshot()->revision, quint64(1));
    QCOMPARE(changed.size(), 1);
    client.stop();
    QVERIFY(!transport.running);
}

void CompositorVisibilityClientTests::
    coalescesBurstsAndAlwaysFollowsAnInFlightInvalidation()
{
    FakeTransport transport;
    ShellVisibilityClient::CompositorVisibilityClient client(transport, fastTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.11");
    transport.announceOwner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], snapshotPayload(QStringLiteral("epoch-a"), 1));

    transport.invalidate(owner);
    transport.invalidate(owner);
    transport.invalidate(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.invalidate(owner);
    transport.invalidate(owner);
    QTest::qWait(5);
    QCOMPARE(transport.requests.size(), 2);

    transport.reply(transport.requests[1], snapshotPayload(QStringLiteral("epoch-a"), 2));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests[2], snapshotPayload(QStringLiteral("epoch-a"), 2));
    QTest::qWait(8);
    QCOMPARE(transport.requests.size(), 3);
}

void CompositorVisibilityClientTests::ignoresLateRepliesAcrossOwnerChanges()
{
    FakeTransport transport;
    ShellVisibilityClient::CompositorVisibilityClient client(transport, fastTiming());
    QVERIFY(client.start());
    transport.announceOwner(QStringLiteral(":1.20"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    const auto oldRequest = transport.requests[0];

    transport.announceOwner(QStringLiteral(":1.21"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    const auto currentRequest = transport.requests[1];
    transport.reply(oldRequest, snapshotPayload(QStringLiteral("old"), 99));
    QVERIFY(!client.snapshot().has_value());
    transport.reply(currentRequest, snapshotPayload(QStringLiteral("new"), 1));

    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);
    QCOMPARE(client.snapshot()->epoch, QStringLiteral("new"));
    QCOMPARE(client.snapshot()->revision, quint64(1));
}

void CompositorVisibilityClientTests::
    timesOutAndRetriesWithoutParallelRequests()
{
    FakeTransport transport;
    auto timing = fastTiming();
    timing.requestTimeoutMilliseconds = 12;
    ShellVisibilityClient::CompositorVisibilityClient client(transport, timing);
    QVERIFY(client.start());
    transport.announceOwner(QStringLiteral(":1.30"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    QVERIFY(client.requestInFlight());

    QTRY_COMPARE_WITH_TIMEOUT(
        client.lastResult().code,
        ShellVisibility::CompositorVisibilityStateErrorCode::TransportFailure,
        100);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    QVERIFY(client.requestInFlight());
    transport.reply(transport.requests[1],
                    snapshotPayload(QStringLiteral("epoch-a"), 1));
    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);
}

void CompositorVisibilityClientTests::
    malformedPayloadFallsBackThenRecoversFromTheRetainedRevision()
{
    FakeTransport transport;
    ShellVisibilityClient::CompositorVisibilityClient client(transport, fastTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.40");
    transport.announceOwner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    const auto valid = snapshotPayload(QStringLiteral("epoch-a"), 1);
    transport.reply(transport.requests[0], valid);
    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);

    transport.invalidate(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(transport.requests[1], QByteArrayLiteral("{"));
    QTRY_VERIFY_WITH_TIMEOUT(client.safeVisibleRequired(), 100);
    QVERIFY(!client.snapshot().has_value());
    QCOMPARE(client.lastResult().code,
             ShellVisibility::CompositorVisibilityStateErrorCode::SnapshotRejected);

    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests[2], valid);
    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);
    QCOMPARE(client.lastResult().event,
             ShellVisibility::CompositorVisibilityStateEvent::SnapshotRecovered);
}

void CompositorVisibilityClientTests::
    serviceLossForcesSafeVisibleAndCancelsPendingWork()
{
    FakeTransport transport;
    ShellVisibilityClient::CompositorVisibilityClient client(transport, fastTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.50");
    transport.announceOwner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], snapshotPayload(QStringLiteral("epoch-a"), 1));
    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);

    transport.invalidate(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    const auto abandoned = transport.requests[1];
    transport.announceOwner({});
    QVERIFY(client.safeVisibleRequired());
    QVERIFY(!client.snapshot().has_value());
    QVERIFY(!client.requestInFlight());

    transport.reply(abandoned, snapshotPayload(QStringLiteral("epoch-a"), 2));
    QTest::qWait(10);
    QVERIFY(!client.snapshot().has_value());
    QCOMPARE(transport.requests.size(), 2);
}

void CompositorVisibilityClientTests::staleOwnerInvalidationsAreIgnored()
{
    FakeTransport transport;
    ShellVisibilityClient::CompositorVisibilityClient client(transport, fastTiming());
    QVERIFY(client.start());
    transport.announceOwner(QStringLiteral(":1.60"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], snapshotPayload(QStringLiteral("epoch-a"), 1));
    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);

    transport.invalidate(QStringLiteral(":1.59"));
    QTest::qWait(10);
    QCOMPARE(transport.requests.size(), 1);
}

void CompositorVisibilityClientTests::rejectsInvalidTimingAndTransportStartup()
{
    FakeTransport transport;
    auto invalid = fastTiming();
    invalid.retryMilliseconds = {10, 5};
    ShellVisibilityClient::CompositorVisibilityClient invalidClient(transport, invalid);
    QString error;
    QVERIFY(!invalidClient.start(&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(transport.startCalls, 0);

    FakeTransport failedTransport;
    failedTransport.startSucceeds = false;
    ShellVisibilityClient::CompositorVisibilityClient failedClient(
        failedTransport, fastTiming());
    error.clear();
    QVERIFY(!failedClient.start(&error));
    QCOMPARE(error, QStringLiteral("injected start failure"));
    QVERIFY(!failedTransport.running);
}

QTEST_GUILESS_MAIN(CompositorVisibilityClientTests)
#include "tst_compositor_visibility_client.moc"

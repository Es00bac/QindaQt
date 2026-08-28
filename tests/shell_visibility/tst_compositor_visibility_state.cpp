// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_visibility/compositor_visibility_state.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <limits>

using namespace QindaQt::ShellVisibility;

namespace {

QByteArray snapshotPayload(QString epoch, quint64 revision, int windowX = 10)
{
    const auto rectangle = [](int x, int y, int width, int height) {
        return QJsonObject{{QStringLiteral("x"), x},
                           {QStringLiteral("y"), y},
                           {QStringLiteral("width"), width},
                           {QStringLiteral("height"), height}};
    };
    const QJsonObject output{{QStringLiteral("id"), QStringLiteral("DP-1")},
                             {QStringLiteral("geometry"), rectangle(0, 0, 1920, 1080)},
                             {QStringLiteral("scale"), 1.0}};
    const QJsonObject window{
        {QStringLiteral("id"), QStringLiteral("window-1")},
        {QStringLiteral("outputId"), QStringLiteral("DP-1")},
        {QStringLiteral("frameGeometry"), rectangle(windowX, 20, 800, 600)},
        {QStringLiteral("workspaceIds"), QJsonArray{QStringLiteral("workspace-1")}},
        {QStringLiteral("onAllWorkspaces"), false},
        {QStringLiteral("activityIds"), QJsonArray{}},
        {QStringLiteral("active"), true},
        {QStringLiteral("maximized"), false},
        {QStringLiteral("minimized"), false},
        {QStringLiteral("hidden"), false},
    };
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("status"), QStringLiteral("ok")},
                             {QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("epoch"), std::move(epoch)},
                             {QStringLiteral("revision"), QString::number(revision)},
                             {QStringLiteral("outputGeneration"), QStringLiteral("1")},
                             {QStringLiteral("scope"),
                              QJsonObject{{QStringLiteral("workspaceId"),
                                           QStringLiteral("workspace-1")},
                                          {QStringLiteral("activityId"),
                                           QStringLiteral("activity-1")}}},
                             {QStringLiteral("outputs"), QJsonArray{output}},
                             {QStringLiteral("windows"), QJsonArray{window}},
                         })
        .toJson(QJsonDocument::Compact);
}

QByteArray unavailablePayload()
{
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("status"), QStringLiteral("unavailable")},
                             {QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("epoch"), QStringLiteral("epoch-a")},
                             {QStringLiteral("revision"), QStringLiteral("1")},
                         })
        .toJson(QJsonDocument::Compact);
}

CompositorVisibilityRequestTag observe(
    CompositorVisibilitySnapshotStateMachine &state, const QString &owner)
{
    const auto result = state.observeServiceOwner(owner);
    if (result.event != CompositorVisibilityStateEvent::OwnerChanged ||
        result.code != CompositorVisibilityStateErrorCode::None) {
        QTest::qFail("service owner was not accepted", __FILE__, __LINE__);
        return {};
    }
    const auto tag = state.currentRequestTag();
    if (!tag.has_value()) {
        QTest::qFail("accepted owner did not produce a request tag", __FILE__, __LINE__);
        return {};
    }
    return *tag;
}

} // namespace

class CompositorVisibilityStateTests final : public QObject {
    Q_OBJECT

private slots:
    void tracksCanonicalServiceOwnersAndRequestGenerations();
    void acceptsMonotonicSnapshotsAndSkipsExactReplays();
    void fallsBackOnRevisionRegressionAndRecoversFromTheRetainedLineage();
    void rejectsRevisionCollisions();
    void startsANewLineageForANewPublisherEpoch();
    void ignoresEveryLateResultFromAnOlderOwner();
    void convergesToSafeVisibleForMalformedUnavailableAndTransportFailures();
    void serviceLossAndInvalidOwnersClearEveryPublicValue();
    void rejectsOwnerGenerationExhaustionWithoutWrapping();
};

void CompositorVisibilityStateTests::
    tracksCanonicalServiceOwnersAndRequestGenerations()
{
    CompositorVisibilitySnapshotStateMachine state;
    QVERIFY(state.safeVisibleRequired());
    QVERIFY(!state.currentRequestTag().has_value());

    const auto first = observe(state, QStringLiteral(":1.42"));
    QCOMPARE(first.ownerGeneration, quint64(1));
    const auto unchanged = state.observeServiceOwner(QStringLiteral(":1.42"));
    QCOMPARE(unchanged.event, CompositorVisibilityStateEvent::NoChange);
    QVERIFY(!unchanged.stateChanged);
    QCOMPARE(state.currentRequestTag(), std::optional(first));

    const auto second = observe(state, QStringLiteral(":1.43"));
    QCOMPARE(second.ownerGeneration, quint64(2));
    QVERIFY(state.safeVisibleRequired());
    QVERIFY(!state.snapshot().has_value());
}

void CompositorVisibilityStateTests::
    acceptsMonotonicSnapshotsAndSkipsExactReplays()
{
    CompositorVisibilitySnapshotStateMachine state;
    const auto request = observe(state, QStringLiteral(":1.50"));

    const auto first = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 1));
    QCOMPARE(first.event, CompositorVisibilityStateEvent::SnapshotAccepted);
    QVERIFY(first.ok());
    QVERIFY(first.stateChanged);
    QVERIFY(!state.safeVisibleRequired());
    QCOMPARE(state.snapshot()->revision, quint64(1));

    const auto replay = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 1));
    QCOMPARE(replay.event, CompositorVisibilityStateEvent::SnapshotUnchanged);
    QVERIFY(!replay.stateChanged);

    const auto next = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 2, 30));
    QCOMPARE(next.event, CompositorVisibilityStateEvent::SnapshotAccepted);
    QVERIFY(next.stateChanged);
    QCOMPARE(state.snapshot()->revision, quint64(2));

    const auto gap = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 7, 40));
    QVERIFY(gap.ok());
    QVERIFY(gap.stateChanged);
    QCOMPARE(state.snapshot()->revision, quint64(7));
}

void CompositorVisibilityStateTests::
    fallsBackOnRevisionRegressionAndRecoversFromTheRetainedLineage()
{
    CompositorVisibilitySnapshotStateMachine state;
    const auto request = observe(state, QStringLiteral(":1.51"));
    QVERIFY(state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 2)).ok());

    const auto regression = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 1));
    QCOMPARE(regression.event, CompositorVisibilityStateEvent::SafeVisibleFallback);
    QCOMPARE(regression.code,
             CompositorVisibilityStateErrorCode::RevisionRegression);
    QVERIFY(regression.stateChanged);
    QVERIFY(state.safeVisibleRequired());
    QVERIFY(!state.snapshot().has_value());
    QCOMPARE(state.lastAcceptedLineage()->revision, quint64(2));

    const auto recovered = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 2));
    QCOMPARE(recovered.event, CompositorVisibilityStateEvent::SnapshotRecovered);
    QVERIFY(recovered.ok());
    QVERIFY(recovered.stateChanged);
    QVERIFY(state.snapshot().has_value());
}

void CompositorVisibilityStateTests::rejectsRevisionCollisions()
{
    CompositorVisibilitySnapshotStateMachine state;
    const auto request = observe(state, QStringLiteral(":1.52"));
    const auto original = snapshotPayload(QStringLiteral("epoch-a"), 3, 10);
    QVERIFY(state.acceptSnapshot(request, original).ok());

    const auto collision = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 3, 90));
    QCOMPARE(collision.code, CompositorVisibilityStateErrorCode::RevisionCollision);
    QVERIFY(collision.stateChanged);
    QVERIFY(state.safeVisibleRequired());

    const auto repeatedCollision = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 3, 90));
    QCOMPARE(repeatedCollision.code,
             CompositorVisibilityStateErrorCode::RevisionCollision);
    QVERIFY(!repeatedCollision.stateChanged);

    const auto recovered = state.acceptSnapshot(request, original);
    QCOMPARE(recovered.event, CompositorVisibilityStateEvent::SnapshotRecovered);
    QVERIFY(!state.safeVisibleRequired());
}

void CompositorVisibilityStateTests::startsANewLineageForANewPublisherEpoch()
{
    CompositorVisibilitySnapshotStateMachine state;
    const auto request = observe(state, QStringLiteral(":1.53"));
    QVERIFY(state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 10)).ok());

    const auto reset = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-b"), 1));

    QCOMPARE(reset.event, CompositorVisibilityStateEvent::SnapshotAccepted);
    QVERIFY(reset.stateChanged);
    QCOMPARE(state.lastAcceptedLineage()->epoch, QStringLiteral("epoch-b"));
    QCOMPARE(state.lastAcceptedLineage()->revision, quint64(1));
}

void CompositorVisibilityStateTests::ignoresEveryLateResultFromAnOlderOwner()
{
    CompositorVisibilitySnapshotStateMachine state;
    const auto oldRequest = observe(state, QStringLiteral(":1.60"));
    QVERIFY(state.acceptSnapshot(
        oldRequest, snapshotPayload(QStringLiteral("epoch-old"), 4)).ok());
    const auto currentRequest = observe(state, QStringLiteral(":1.61"));
    QVERIFY(state.acceptSnapshot(
        currentRequest, snapshotPayload(QStringLiteral("epoch-new"), 2)).ok());
    const auto retained = state.snapshot();

    const auto latePayload = state.acceptSnapshot(
        oldRequest, snapshotPayload(QStringLiteral("epoch-old"), 5));
    QVERIFY(latePayload.stale());
    QVERIFY(!latePayload.stateChanged);
    QCOMPARE(state.snapshot(), retained);

    const auto lateFailure = state.requestFailed(
        oldRequest, QStringLiteral("late transport error"));
    QVERIFY(lateFailure.stale());
    QCOMPARE(state.snapshot(), retained);

    const auto lateLoss = state.serviceLost(oldRequest);
    QVERIFY(lateLoss.stale());
    QCOMPARE(state.currentRequestTag(), std::optional(currentRequest));
    QCOMPARE(state.snapshot(), retained);
}

void CompositorVisibilityStateTests::
    convergesToSafeVisibleForMalformedUnavailableAndTransportFailures()
{
    CompositorVisibilitySnapshotStateMachine state;
    const auto request = observe(state, QStringLiteral(":1.70"));
    QVERIFY(state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 1)).ok());

    auto fallback = state.acceptSnapshot(request, QByteArrayLiteral("{"));
    QCOMPARE(fallback.code, CompositorVisibilityStateErrorCode::SnapshotRejected);
    QCOMPARE(fallback.snapshotError.code, CompositorSnapshotErrorCode::InvalidJson);
    QVERIFY(fallback.stateChanged);
    QVERIFY(state.safeVisibleRequired());

    fallback = state.acceptSnapshot(request, unavailablePayload());
    QCOMPARE(fallback.code, CompositorVisibilityStateErrorCode::SnapshotRejected);
    QVERIFY(!fallback.stateChanged);

    const auto recovered = state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 1));
    QCOMPARE(recovered.event, CompositorVisibilityStateEvent::SnapshotRecovered);

    fallback = state.requestFailed(request, QStringLiteral("timed out"));
    QCOMPARE(fallback.code, CompositorVisibilityStateErrorCode::TransportFailure);
    QVERIFY(fallback.stateChanged);
    QVERIFY(state.safeVisibleRequired());
    QVERIFY(!state.snapshot().has_value());
}

void CompositorVisibilityStateTests::
    serviceLossAndInvalidOwnersClearEveryPublicValue()
{
    CompositorVisibilitySnapshotStateMachine state;
    auto request = observe(state, QStringLiteral(":1.80"));
    QVERIFY(state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-a"), 1)).ok());

    const auto lost = state.serviceLost(request);
    QCOMPARE(lost.code, CompositorVisibilityStateErrorCode::ServiceLost);
    QVERIFY(lost.stateChanged);
    QVERIFY(!state.currentRequestTag().has_value());
    QVERIFY(!state.lastAcceptedLineage().has_value());
    QVERIFY(!state.snapshot().has_value());
    QVERIFY(state.safeVisibleRequired());

    request = observe(state, QStringLiteral(":1.81"));
    QVERIFY(state.acceptSnapshot(
        request, snapshotPayload(QStringLiteral("epoch-b"), 1)).ok());
    const auto invalid = state.observeServiceOwner(QStringLiteral("org.qindaqt.Compositor1"));
    QCOMPARE(invalid.code, CompositorVisibilityStateErrorCode::InvalidUniqueOwner);
    QVERIFY(invalid.stateChanged);
    QVERIFY(!state.currentRequestTag().has_value());
    QVERIFY(!state.snapshot().has_value());
}

void CompositorVisibilityStateTests::
    rejectsOwnerGenerationExhaustionWithoutWrapping()
{
    CompositorVisibilitySnapshotStateMachine state(
        std::numeric_limits<quint64>::max());

    const auto result = state.observeServiceOwner(QStringLiteral(":1.90"));

    QCOMPARE(result.code,
             CompositorVisibilityStateErrorCode::OwnerGenerationExhausted);
    QVERIFY(state.safeVisibleRequired());
    QVERIFY(!state.currentRequestTag().has_value());
}

QTEST_GUILESS_MAIN(CompositorVisibilityStateTests)
#include "tst_compositor_visibility_state.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/shellvisibilitysnapshot.h"
#include "qindaqt/shell_visibility/compositor_visibility_snapshot.h"

#include <QtTest>

#include <type_traits>

using namespace QindaQt;

static_assert(std::is_same_v<Compositor::ShellVisibilityWireLimits,
                             ShellVisibility::CompositorVisibilitySnapshotDecoder::WireLimits>);

class CompositorVisibilityWireRoundtripTests final : public QObject {
    Q_OBJECT

private slots:
    void producerPayloadDecodesWithoutLoss();
    void maximumSharedIdentifierAndScaleRemainInteroperable();
    void unavailablePublicationForcesConsumerRejection();
};

void CompositorVisibilityWireRoundtripTests::producerPayloadDecodesWithoutLoss()
{
    Compositor::ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    const Compositor::ShellVisibilitySnapshotCandidate candidate{
        .scope = {QStringLiteral("workspace-1"), QStringLiteral("activity-1")},
        .outputs = {{QStringLiteral("DP-1"), QRect(-1920, 0, 1920, 1080), 1.25}},
        .windows = {{.id = QStringLiteral("window-1"),
                     .outputId = QStringLiteral("DP-1"),
                     .frameGeometry = QRect(-1800, 50, 800, 600),
                     .workspaceIds = {QStringLiteral("workspace-2"),
                                      QStringLiteral("workspace-1")},
                     .activityIds = {},
                     .active = true,
                     .maximized = true}},
    };

    QCOMPARE(store.publish(candidate),
             Compositor::ShellVisibilityPublishResult::Published);
    const auto decoded =
        ShellVisibility::CompositorVisibilitySnapshotDecoder::decode(
            store.snapshotJson());

    QVERIFY2(decoded.ok(), qPrintable(decoded.error.message));
    QCOMPARE(decoded.snapshot->epoch, QStringLiteral("epoch-a"));
    QCOMPARE(decoded.snapshot->revision, quint64(1));
    QCOMPARE(decoded.snapshot->scope.workspaceId, QStringLiteral("workspace-1"));
    QCOMPARE(decoded.snapshot->outputs.constFirst().geometry,
             QRect(-1920, 0, 1920, 1080));
    QCOMPARE(decoded.snapshot->outputs.constFirst().scale, 1.25);
    QCOMPARE(decoded.snapshot->windows.constFirst().workspaceIds,
             QStringList({QStringLiteral("workspace-1"),
                          QStringLiteral("workspace-2")}));
    QVERIFY(decoded.snapshot->windows.constFirst().active);
    QVERIFY(decoded.snapshot->windows.constFirst().maximized);
}

void CompositorVisibilityWireRoundtripTests::
    maximumSharedIdentifierAndScaleRemainInteroperable()
{
    using Limits = Compositor::ShellVisibilityWireLimits;
    const QString identifier(Limits::MaxIdentifierCharacters, QLatin1Char('x'));
    Compositor::ShellVisibilitySnapshotStore store(identifier);
    const Compositor::ShellVisibilitySnapshotCandidate candidate{
        .scope = {identifier, identifier},
        .outputs = {{identifier, QRect(0, 0, 1, 1), Limits::MaxOutputScale}},
        .windows = {},
    };

    QCOMPARE(store.publish(candidate),
             Compositor::ShellVisibilityPublishResult::Published);
    const auto decoded =
        ShellVisibility::CompositorVisibilitySnapshotDecoder::decode(
            store.snapshotJson());

    QVERIFY2(decoded.ok(), qPrintable(decoded.error.message));
    QCOMPARE(decoded.snapshot->epoch.size(), Limits::MaxIdentifierCharacters);
    QCOMPARE(decoded.snapshot->outputs.constFirst().scale,
             Limits::MaxOutputScale);
}

void CompositorVisibilityWireRoundtripTests::
    unavailablePublicationForcesConsumerRejection()
{
    Compositor::ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    const Compositor::ShellVisibilitySnapshotCandidate candidate{
        .scope = {QStringLiteral("workspace-1"), QStringLiteral("activity-1")},
        .outputs = {{QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0}},
        .windows = {},
    };
    QCOMPARE(store.publish(candidate),
             Compositor::ShellVisibilityPublishResult::Published);
    QVERIFY(store.markUnavailable(QStringLiteral("snapshot-invalid"),
                                  QStringLiteral("coherent sample failed")));

    const auto decoded =
        ShellVisibility::CompositorVisibilitySnapshotDecoder::decode(
            store.snapshotJson());

    QVERIFY(!decoded.ok());
    QCOMPARE(decoded.error.code,
             ShellVisibility::CompositorSnapshotErrorCode::InvalidRoot);
}

QTEST_GUILESS_MAIN(CompositorVisibilityWireRoundtripTests)
#include "tst_compositor_visibility_wire_roundtrip.moc"

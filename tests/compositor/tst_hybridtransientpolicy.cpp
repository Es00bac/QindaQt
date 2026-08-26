// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridtransientpolicy.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

TransientSnapshot dialog(QString transientId = QStringLiteral("dialog"),
                         QString ownerId = QStringLiteral("left"))
{
    return {
        .transientId = std::move(transientId),
        .ownerWindowId = std::move(ownerId),
        .ownerContainerId = QStringLiteral("group"),
        .transientFrame = QRectF(160.0, 180.0, 360.0, 240.0),
        .ownerFrame = QRectF(100.0, 100.0, 500.0, 600.0),
    };
}

} // namespace

class HybridTransientPolicyTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void associatesOnlyNonTiledTransientsWithGroupedOwners();
    void followsOwnerByStableCumulativeOffset();
    void preservesUserAdjustedDialogOffset();
    void replacesAndRemovesAssociationsDeterministically();
};

void HybridTransientPolicyTest::associatesOnlyNonTiledTransientsWithGroupedOwners()
{
    HybridTransientPolicy policy;
    const QSet<QString> members{QStringLiteral("left"), QStringLiteral("right")};
    QString error;

    QVERIFY(policy.synchronize({dialog()}, members, &error));
    const auto association = policy.association(QStringLiteral("dialog"));
    QVERIFY(association);
    QCOMPARE(association->ownerWindowId, QStringLiteral("left"));
    QCOMPARE(association->ownerContainerId, QStringLiteral("group"));
    QCOMPARE(association->ownerOffset, QPointF(60.0, 80.0));

    auto tiledDialog = dialog(QStringLiteral("right"));
    QVERIFY(!policy.synchronize({tiledDialog}, members, &error));
    QVERIFY(error.contains(QStringLiteral("tiled member")));

    auto independentOwner = dialog(QStringLiteral("dialog-2"),
                                   QStringLiteral("independent"));
    QVERIFY(!policy.synchronize({independentOwner}, members, &error));
    QVERIFY(error.contains(QStringLiteral("not grouped")));
}

void HybridTransientPolicyTest::followsOwnerByStableCumulativeOffset()
{
    HybridTransientPolicy policy;
    QVERIFY(policy.synchronize({dialog()}, {QStringLiteral("left")}));

    auto placements = policy.ownerFrameChanged(
        QStringLiteral("left"), QRectF(300.0, 250.0, 500.0, 600.0));
    QCOMPARE(placements.size(), 1);
    QCOMPARE(placements[0].frame, QRectF(360.0, 330.0, 360.0, 240.0));

    // A second notification is total owner displacement from the association
    // baseline, not an increment accumulated from the last transient frame.
    placements = policy.ownerFrameChanged(
        QStringLiteral("left"), QRectF(475.0, 390.0, 500.0, 600.0));
    QCOMPARE(placements[0].frame, QRectF(535.0, 470.0, 360.0, 240.0));
    QCOMPARE(policy.association(QStringLiteral("dialog"))->ownerOffset,
             QPointF(60.0, 80.0));
}

void HybridTransientPolicyTest::preservesUserAdjustedDialogOffset()
{
    HybridTransientPolicy policy;
    QVERIFY(policy.synchronize({dialog()}, {QStringLiteral("left")}));
    QVERIFY(policy.transientFrameChanged(
        QStringLiteral("dialog"), QRectF(220.0, 260.0, 420.0, 280.0)));
    QCOMPARE(policy.association(QStringLiteral("dialog"))->ownerOffset,
             QPointF(120.0, 160.0));

    const auto placements = policy.ownerFrameChanged(
        QStringLiteral("left"), QRectF(500.0, 500.0, 500.0, 600.0));
    QCOMPARE(placements[0].frame, QRectF(620.0, 660.0, 420.0, 280.0));
}

void HybridTransientPolicyTest::replacesAndRemovesAssociationsDeterministically()
{
    HybridTransientPolicy policy;
    auto second = dialog(QStringLiteral("chooser"), QStringLiteral("right"));
    second.transientFrame = QRectF(700.0, 200.0, 240.0, 180.0);
    second.ownerFrame = QRectF(600.0, 100.0, 400.0, 600.0);
    const QSet<QString> members{QStringLiteral("left"), QStringLiteral("right")};
    QVERIFY(policy.synchronize({dialog(), second}, members));
    QCOMPARE(policy.associations().size(), 2);

    QVERIFY(policy.synchronize({second}, members));
    QCOMPARE(policy.associations().size(), 1);
    QVERIFY(!policy.association(QStringLiteral("dialog")));
    QCOMPARE(policy.associations().constFirst().transientId,
             QStringLiteral("chooser"));

    QString error;
    QVERIFY(!policy.synchronize({second, second}, members, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate")));
    QCOMPARE(policy.associations().size(), 1);
    QCOMPARE(policy.associations().constFirst().transientId,
             QStringLiteral("chooser"));
}

QTEST_GUILESS_MAIN(HybridTransientPolicyTest)

#include "tst_hybridtransientpolicy.moc"

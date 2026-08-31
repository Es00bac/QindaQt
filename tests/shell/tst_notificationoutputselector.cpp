// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositoroutputauthority.h"
#include "notificationoutputselector.h"
#include "notification_output_test_support.h"

#include <QJsonDocument>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell;

namespace {

ShellLayout::LogicalOutput qtOutput(const QString &id, const QRect &geometry)
{
    return {id, geometry, 1.0};
}

ShellVisibility::LogicalOutputSnapshot visibilityOutput(
    const QString &id, const QRect &geometry)
{
    return {id, geometry, 1.0};
}

CompositorOutputAuthorityFrame authority(
    quint64 generation, std::initializer_list<QString> semanticOrder)
{
    CompositorOutputAuthorityFrame result;
    result.uniqueOwner = QStringLiteral(":1.42");
    result.outputGeneration = generation;
    quint32 priority = 1;
    for (const auto &id : semanticOrder) {
        result.outputs.append({id, priority++});
    }
    return result;
}

ShellVisibility::CompositorVisibilitySnapshot visibility(
    quint64 generation,
    std::initializer_list<ShellVisibility::LogicalOutputSnapshot> outputs)
{
    ShellVisibility::CompositorVisibilitySnapshot result;
    result.epoch = QStringLiteral("feedbeef-feed-beef-feed-beeffeedbeef");
    result.revision = generation;
    result.outputGeneration = generation;
    result.outputs = QVector<ShellVisibility::LogicalOutputSnapshot>(outputs);
    result.scope = {QStringLiteral("workspace-1"), QStringLiteral("activity-1")};
    return result;
}

} // namespace

class NotificationOutputSelectorTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void decoderPreservesPublicSemanticOrder();
    void primaryTransferOverridesStaleQtPrimary();
    void replacementAndRemovalFollowExactGeneration();
    void noMatchFailsClosed();
    void unchangedPrimaryRetainsTheSameRoute();
    void hostileAuthorityPayloadsRejectAtomically();
};

void NotificationOutputSelectorTests::decoderPreservesPublicSemanticOrder()
{
    const auto decoded = CompositorOutputAuthorityDecoder::decode(
        TestSupport::authorityPayload(
            7, {TestSupport::wireOutput(QStringLiteral("WL-1"), 1),
                TestSupport::wireOutput(QStringLiteral("WL-0"), 2)}),
        QStringLiteral(":1.42"));
    QVERIFY2(decoded.ok(), qPrintable(decoded.message));
    QCOMPARE(decoded.frame->uniqueOwner, QStringLiteral(":1.42"));
    QCOMPARE(decoded.frame->outputGeneration, quint64(7));
    QCOMPARE(decoded.frame->outputs.size(), 2);
    QCOMPARE(decoded.frame->outputs.at(0).outputId, QStringLiteral("WL-1"));
    QCOMPARE(decoded.frame->outputs.at(1).outputId, QStringLiteral("WL-0"));
}

void NotificationOutputSelectorTests::primaryTransferOverridesStaleQtPrimary()
{
    const QRect left(0, 0, 1920, 1080);
    const QRect right(1920, 0, 1920, 1080);
    const auto snapshot = visibility(
        2, {visibilityOutput(QStringLiteral("WL-0"), left),
            visibilityOutput(QStringLiteral("WL-1"), right)});
    const QVector<ShellLayout::LogicalOutput> qtOutputs{
        qtOutput(QStringLiteral("WL-0"), left),
        qtOutput(QStringLiteral("WL-1"), right),
    };
    const std::optional frame = authority(
        2, {QStringLiteral("WL-1"), QStringLiteral("WL-0")});

    const auto selected =
        NotificationOutputSelector::select(frame, &snapshot, qtOutputs);
    QVERIFY2(selected.ok(), qPrintable(selected.message));
    QCOMPARE(selected.outputId, QStringLiteral("WL-1"));

    // Mutation-sensitive control: this is exactly the value returned by the
    // former QGuiApplication::primaryScreen() route in the S3 reproduction.
    const QString staleQtPrimary = QStringLiteral("WL-0");
    QVERIFY(selected.outputId != staleQtPrimary);
}

void NotificationOutputSelectorTests::replacementAndRemovalFollowExactGeneration()
{
    const QRect left(0, 0, 1920, 1080);
    const QRect right(1920, 0, 1920, 1080);
    auto snapshot = visibility(
        8, {visibilityOutput(QStringLiteral("WL-2"), left),
            visibilityOutput(QStringLiteral("WL-1"), right)});
    QVector<ShellLayout::LogicalOutput> qtOutputs{
        qtOutput(QStringLiteral("WL-2"), left),
        qtOutput(QStringLiteral("WL-1"), right),
    };
    std::optional frame = authority(
        8, {QStringLiteral("WL-2"), QStringLiteral("WL-1")});
    auto selected = NotificationOutputSelector::select(frame, &snapshot, qtOutputs);
    QVERIFY2(selected.ok(), qPrintable(selected.message));
    QCOMPARE(selected.outputId, QStringLiteral("WL-2"));

    snapshot = visibility(
        9, {visibilityOutput(QStringLiteral("WL-1"), left)});
    qtOutputs = {qtOutput(QStringLiteral("WL-1"), left)};
    frame = authority(9, {QStringLiteral("WL-1")});
    selected = NotificationOutputSelector::select(frame, &snapshot, qtOutputs);
    QVERIFY2(selected.ok(), qPrintable(selected.message));
    QCOMPARE(selected.outputId, QStringLiteral("WL-1"));
}

void NotificationOutputSelectorTests::noMatchFailsClosed()
{
    const QRect geometry(0, 0, 1920, 1080);
    auto snapshot = visibility(
        4, {visibilityOutput(QStringLiteral("WL-0"), geometry)});
    const QVector<ShellLayout::LogicalOutput> qtOutputs{
        qtOutput(QStringLiteral("WL-0"), geometry),
    };
    std::optional frame = authority(3, {QStringLiteral("WL-0")});

    auto selected = NotificationOutputSelector::select(frame, &snapshot, qtOutputs);
    QVERIFY(!selected.ok());
    QCOMPARE(selected.error, NotificationOutputSelectionError::GenerationMismatch);

    frame = authority(4, {QStringLiteral("WL-1")});
    selected = NotificationOutputSelector::select(frame, &snapshot, qtOutputs);
    QVERIFY(!selected.ok());
    QCOMPARE(selected.error, NotificationOutputSelectionError::InventoryMismatch);

    selected = NotificationOutputSelector::select({}, &snapshot, qtOutputs);
    QVERIFY(!selected.ok());
    QCOMPARE(selected.error, NotificationOutputSelectionError::MissingAuthority);

    selected = NotificationOutputSelector::select(
        authority(4, {QStringLiteral("WL-0")}), nullptr, qtOutputs);
    QVERIFY(!selected.ok());
    QCOMPARE(selected.error, NotificationOutputSelectionError::MissingVisibility);
}

void NotificationOutputSelectorTests::unchangedPrimaryRetainsTheSameRoute()
{
    const QRect left(0, 0, 1920, 1080);
    const QRect right(1920, 0, 1920, 1080);
    auto snapshot = visibility(
        10, {visibilityOutput(QStringLiteral("WL-0"), left),
             visibilityOutput(QStringLiteral("WL-1"), right)});
    const QVector<ShellLayout::LogicalOutput> qtOutputs{
        qtOutput(QStringLiteral("WL-0"), left),
        qtOutput(QStringLiteral("WL-1"), right),
    };
    auto first = NotificationOutputSelector::select(
        authority(10, {QStringLiteral("WL-0"), QStringLiteral("WL-1")}),
        &snapshot, qtOutputs);
    QVERIFY2(first.ok(), qPrintable(first.message));

    snapshot.outputGeneration = 11;
    snapshot.revision = 11;
    auto refreshed = NotificationOutputSelector::select(
        authority(11, {QStringLiteral("WL-0"), QStringLiteral("WL-1")}),
        &snapshot, qtOutputs);
    QVERIFY2(refreshed.ok(), qPrintable(refreshed.message));
    QCOMPARE(first.outputId, QStringLiteral("WL-0"));
    QCOMPARE(refreshed.outputId, first.outputId);
}

void NotificationOutputSelectorTests::hostileAuthorityPayloadsRejectAtomically()
{
    const QByteArray good = TestSupport::authorityPayload(
        1, {TestSupport::wireOutput(QStringLiteral("WL-0"), 1)});
    auto decoded = CompositorOutputAuthorityDecoder::decode(
        good, QStringLiteral("org.qindaqt.Compositor"));
    QVERIFY(!decoded.ok());
    QCOMPARE(decoded.error, CompositorOutputAuthorityDecodeError::InvalidOwner);

    QJsonObject duplicateRoot = QJsonDocument::fromJson(
        TestSupport::authorityPayload(
            1, {TestSupport::wireOutput(QStringLiteral("WL-0"), 1),
                TestSupport::wireOutput(QStringLiteral("WL-0"), 2)})).object();
    decoded = CompositorOutputAuthorityDecoder::decode(
        QJsonDocument(duplicateRoot).toJson(QJsonDocument::Compact),
        QStringLiteral(":1.42"));
    QVERIFY(!decoded.ok());
    QCOMPARE(decoded.error, CompositorOutputAuthorityDecodeError::InvalidOutput);

    QJsonObject malformedRoot = QJsonDocument::fromJson(good).object();
    QJsonArray outputs = malformedRoot.value(QStringLiteral("outputs")).toArray();
    QJsonObject malformedOutput = outputs.at(0).toObject();
    malformedOutput[QStringLiteral("scale")] = QStringLiteral("1");
    outputs[0] = malformedOutput;
    malformedRoot[QStringLiteral("outputs")] = outputs;
    decoded = CompositorOutputAuthorityDecoder::decode(
        QJsonDocument(malformedRoot).toJson(QJsonDocument::Compact),
        QStringLiteral(":1.42"));
    QVERIFY(!decoded.ok());
    QCOMPARE(decoded.error, CompositorOutputAuthorityDecodeError::InvalidOutput);
}

QTEST_APPLESS_MAIN(NotificationOutputSelectorTests)
#include "tst_notificationoutputselector.moc"

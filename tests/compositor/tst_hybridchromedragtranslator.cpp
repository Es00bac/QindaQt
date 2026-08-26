// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromedragtranslator.h"

#include <QtTest>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

class StubResolver final : public HybridInput::InteractionTargetResolver
{
public:
    HybridInput::HitTarget hitTest(const QPointF &) const override { return {}; }

    HybridInput::DockTarget pointerDockTarget(
        const HybridInput::HitTarget &source, const QPointF &position) const override
    {
        lastSource = source;
        lastPosition = position;
        ++pointerCalls;
        return pointerResult;
    }

    HybridInput::DockTarget keyboardDockTarget(
        const HybridInput::HitTarget &, HybridInput::DockZone) const override
    {
        return {};
    }

    mutable HybridInput::HitTarget lastSource;
    mutable QPointF lastPosition;
    mutable int pointerCalls = 0;
    HybridInput::DockTarget pointerResult{
        QStringLiteral("other-group"), QStringLiteral("target"),
        HybridInput::DockZone::Right};
};

Hybrid::WindowTopology topology()
{
    Core::WindowContainer container(QStringLiteral("group"));
    QString error;
    if (!container.addPage(QStringLiteral("work"), QStringLiteral("left-leaf"),
                           QStringLiteral("left"), &error)
        || !container.splitWindow({.targetWindowId = QStringLiteral("left"),
                                   .newWindowId = QStringLiteral("right"),
                                   .newLeafNodeId = QStringLiteral("right-leaf"),
                                   .splitNodeId = QStringLiteral("divider"),
                                   .orientation = Core::SplitOrientation::Horizontal,
                                   .ratio = 0.5,
                                   .position = Core::InsertPosition::Second},
                                  &error)
        || !container.addPage(QStringLiteral("chat"), QStringLiteral("chat-leaf"),
                              QStringLiteral("chat"), &error)) {
        qFatal("could not build translation fixture: %s", qPrintable(error));
    }
    auto result = Hybrid::WindowTopology::create(
        {QStringLiteral("target")}, {std::move(container)}, 7, &error);
    if (!result) {
        qFatal("could not build topology fixture: %s", qPrintable(error));
    }
    return std::move(*result);
}

HybridChrome::ChromeDragEvent drag(HybridChrome::HitKind kind,
                                   QString stableId,
                                   HybridChrome::DragPhase phase)
{
    return {.target = {.kind = kind,
                       .stableId = std::move(stableId),
                       .logicalIndex = -1,
                       .action = std::nullopt,
                       .resizeEdges = {}},
            .phase = phase,
            .globalPosition = QPointF(700, 400),
            .delta = QPointF(30, -10)};
}

} // namespace

class HybridChromeDragTranslatorTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void translatesMemberUpdateWithResolvedDockTarget();
    void translatesTabUsingStableLogicalRepresentative();
    void translatesOuterMoveAndDividerWithoutTargetResolution();
    void rejectsStaleAndPolicyOwnedTargets();
};

void HybridChromeDragTranslatorTest::translatesMemberUpdateWithResolvedDockTarget()
{
    StubResolver resolver;
    HybridChromeDragTranslator translator(resolver);
    QString error;
    const auto intent = translator.translate(
        topology(), QStringLiteral("group"),
        drag(HybridChrome::HitKind::MemberTitleDrag, QStringLiteral("right"),
             HybridChrome::DragPhase::Update),
        &error);

    QVERIFY2(intent.has_value(), qPrintable(error));
    QCOMPARE(intent->kind, HybridInput::InteractionKind::MemberDock);
    QCOMPARE(intent->phase, HybridInput::IntentPhase::Update);
    QCOMPARE(intent->source,
             (HybridInput::HitTarget{HybridInput::HitKind::MemberTitle,
                                     QStringLiteral("group"), QStringLiteral("right"), {}}));
    QCOMPARE(intent->target, resolver.pointerResult);
    QCOMPARE(intent->delta, QPointF(30, -10));
    QCOMPARE(resolver.pointerCalls, 1);
}

void HybridChromeDragTranslatorTest::translatesTabUsingStableLogicalRepresentative()
{
    StubResolver resolver;
    HybridChromeDragTranslator translator(resolver);
    const auto intent = translator.translate(
        topology(), QStringLiteral("group"),
        drag(HybridChrome::HitKind::Tab, QStringLiteral("work"),
             HybridChrome::DragPhase::Commit));

    QVERIFY(intent.has_value());
    QCOMPARE(intent->source.kind, HybridInput::HitKind::Tab);
    QCOMPARE(intent->source.memberId, QStringLiteral("left"));
    QCOMPARE(intent->source.pageId, QStringLiteral("work"));
    QCOMPARE(resolver.lastSource.memberId, QStringLiteral("left"));
    QCOMPARE(resolver.pointerCalls, 1);
}

void HybridChromeDragTranslatorTest::translatesOuterMoveAndDividerWithoutTargetResolution()
{
    StubResolver resolver;
    HybridChromeDragTranslator translator(resolver);
    const auto snapshot = topology();
    const auto move = translator.translate(
        snapshot, QStringLiteral("group"),
        drag(HybridChrome::HitKind::OuterTitleDrag, {}, HybridChrome::DragPhase::Begin));
    const auto divider = translator.translate(
        snapshot, QStringLiteral("group"),
        drag(HybridChrome::HitKind::Divider, QStringLiteral("divider"),
             HybridChrome::DragPhase::Cancel));

    QVERIFY(move.has_value());
    QCOMPARE(move->kind, HybridInput::InteractionKind::ContainerMove);
    QCOMPARE(move->phase, HybridInput::IntentPhase::Begin);
    QVERIFY(divider.has_value());
    QCOMPARE(divider->kind, HybridInput::InteractionKind::DividerResize);
    QCOMPARE(divider->source.dividerId, QStringLiteral("divider"));
    QCOMPARE(divider->phase, HybridInput::IntentPhase::Cancel);
    QCOMPARE(resolver.pointerCalls, 0);
}

void HybridChromeDragTranslatorTest::rejectsStaleAndPolicyOwnedTargets()
{
    StubResolver resolver;
    HybridChromeDragTranslator translator(resolver);
    QString error;
    QVERIFY(!translator.translate(
        topology(), QStringLiteral("group"),
        drag(HybridChrome::HitKind::MemberTitleDrag, QStringLiteral("target"),
             HybridChrome::DragPhase::Commit),
        &error));
    QCOMPARE(error, QStringLiteral("member-title drag ownership is stale"));

    QVERIFY(!translator.translate(
        topology(), QStringLiteral("group"),
        drag(HybridChrome::HitKind::OuterResize, {}, HybridChrome::DragPhase::Update),
        &error));
    QCOMPARE(error, QStringLiteral("chrome target is not a runtime drag interaction"));
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::HybridChromeDragTranslatorTest)

#include "tst_hybridchromedragtranslator.moc"

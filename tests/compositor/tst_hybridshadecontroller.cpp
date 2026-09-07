// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshadecontroller.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

class FakeShadeMemberPlatform final : public HybridShadeMemberPlatform
{
public:
    QStringList hiddenMembers;
    QStringList hiddenAnchorContents;
    QStringList failingWindowIds;

    bool hideMember(const QString &windowId, QString *error) override
    {
        if (failingWindowIds.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel hide failure for %1").arg(windowId);
            }
            return false;
        }
        hiddenMembers.append(windowId);
        return true;
    }

    bool showMember(const QString &windowId, QString *error) override
    {
        if (failingWindowIds.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel show failure for %1").arg(windowId);
            }
            return false;
        }
        hiddenMembers.removeAll(windowId);
        return true;
    }

    bool hideAnchorContent(const QString &windowId, QString *error) override
    {
        if (failingWindowIds.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel anchor-hide failure for %1").arg(windowId);
            }
            return false;
        }
        hiddenAnchorContents.append(windowId);
        return true;
    }

    bool showAnchorContent(const QString &windowId, QString *error) override
    {
        Q_UNUSED(error)
        hiddenAnchorContents.removeAll(windowId);
        return true;
    }
};

} // namespace

class HybridShadeControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void shadesEveryMemberAndOnlyTheAnchorGetsContentHiding();
    void unshadesExactlyWhatWasHiddenRegardlessOfCurrentTopology();
    void isIdempotentAndRejectsShadingAnAlreadyShadedContainer();
    void rejectsAnAnchorThatIsNotAMemberOrAnEmptyMemberList();
    void rollsBackEveryMemberIfOneFailsDuringShade();
    void reportsFailureButStillForgetsTheContainerIfOneMemberFailsDuringUnshade();
    void forgetContainerDropsShadeStateWithoutTouchingThePlatform();
};

void HybridShadeControllerTests::shadesEveryMemberAndOnlyTheAnchorGetsContentHiding()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY2(controller.shadeMembers(
                 QStringLiteral("group"),
                 {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
                 QStringLiteral("b"), &error),
             qPrintable(error));
    QVERIFY(controller.isShaded(QStringLiteral("group")));
    QCOMPARE(platform.hiddenMembers,
             (QStringList{QStringLiteral("a"), QStringLiteral("c")}));
    QCOMPARE(platform.hiddenAnchorContents, QStringList{QStringLiteral("b")});
    // Group stacking reads this to exempt the shaded, non-anchor members
    // from its live-stack contiguity/layer checks (see
    // KWinHybridGroupStacking::synchronize's shadedAnchors parameter).
    QCOMPARE(controller.anchorWindowId(QStringLiteral("group")),
             QStringLiteral("b"));
}

void HybridShadeControllerTests::unshadesExactlyWhatWasHiddenRegardlessOfCurrentTopology()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
    QVERIFY(platform.hiddenMembers.isEmpty());
    QVERIFY(platform.hiddenAnchorContents.isEmpty());
    QVERIFY(controller.anchorWindowId(QStringLiteral("group")).isEmpty());

    // Unshading a container that was never shaded fails cleanly.
    QVERIFY(!controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
}

void HybridShadeControllerTests::isIdempotentAndRejectsShadingAnAlreadyShadedContainer()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    QVERIFY(!controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    QVERIFY(!error.isEmpty());
    // The original hidden set is untouched by the rejected second call.
    QCOMPARE(platform.hiddenMembers, QStringList{QStringLiteral("b")});
}

void HybridShadeControllerTests::rejectsAnAnchorThatIsNotAMemberOrAnEmptyMemberList()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(!controller.shadeMembers(QStringLiteral("group"), {}, {}, &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("not-a-member"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(platform.hiddenMembers.isEmpty());
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
}

void HybridShadeControllerTests::rollsBackEveryMemberIfOneFailsDuringShade()
{
    FakeShadeMemberPlatform platform;
    platform.failingWindowIds.append(QStringLiteral("c"));
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(!controller.shadeMembers(
        QStringLiteral("group"),
        {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
        QStringLiteral("a"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
    // "a" (anchor) and "b" were hidden before "c" failed; both must be
    // restored so a rejected shade never leaves a half-hidden group.
    QVERIFY(platform.hiddenMembers.isEmpty());
    QVERIFY(platform.hiddenAnchorContents.isEmpty());
}

void HybridShadeControllerTests::reportsFailureButStillForgetsTheContainerIfOneMemberFailsDuringUnshade()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"),
        {QStringLiteral("a"), QStringLiteral("b")}, QStringLiteral("a"), &error));

    // A member closed while shaded: its show call now fails.
    platform.failingWindowIds.append(QStringLiteral("b"));
    QVERIFY(!controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
    // The container is no longer tracked as shaded even though one member's
    // restore failed; a stuck container would block every future shade.
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
}

void HybridShadeControllerTests::forgetContainerDropsShadeStateWithoutTouchingThePlatform()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    controller.forgetContainer(QStringLiteral("group"));
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
    QCOMPARE(controller.shadedContainerIds(), QStringList{});
    QVERIFY(controller.anchorWindowId(QStringLiteral("group")).isEmpty());
    // forgetContainer is a bookkeeping-only drop; a real teardown path is
    // expected to call unshadeMembers itself first if it wants members shown
    // again (see KWinHybridSession::forgetShadedContainer).
    QCOMPARE(platform.hiddenMembers, QStringList{QStringLiteral("b")});
}

QTEST_GUILESS_MAIN(HybridShadeControllerTests)
#include "tst_hybridshadecontroller.moc"
